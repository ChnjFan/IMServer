//
// Created by fan on 24-12-29.
//

#include "ServiceHandler.h"
#include "Poco/Net/NetException.h"
#include "Poco/UUIDGenerator.h"
#include "IM.BaseType.pb.h"
#include "ServiceMessage.h"
#include "Exception.h"

ServerNet::ServiceHandler::ServiceHandler(Poco::Net::StreamSocket &socket, Poco::Net::SocketReactor &reactor) :
        _socket(socket),
        _reactor(reactor),
        _buffer(0),
        _or(*this, &ServerNet::ServiceHandler::onReadable),
        _ow(*this, &ServerNet::ServiceHandler::onWritable),
        _oe(*this, &ServerNet::ServiceHandler::onError),
        _ot(*this, &ServerNet::ServiceHandler::onTimeout),
        _os(*this, &ServerNet::ServiceHandler::onShutdown)
{
    _buffer.setCapacity(SOCKET_BUFFER_LEN);
    _reactor.addEventHandler(_socket, _ot);
    _reactor.addEventHandler(_socket, _oe);
    _reactor.addEventHandler(_socket, _os);
    _reactor.addEventHandler(_socket, _or);
    _reactor.addEventHandler(_socket, _ow);

    // 为客户端连接生成唯一ID
    generateUid();
    // 建立连接后回复客户端 UID
    responseUid();
}

ServerNet::ServiceHandler::~ServiceHandler() {
    _reactor.removeEventHandler(_socket, _ot);
    _reactor.removeEventHandler(_socket, _oe);
    _reactor.removeEventHandler(_socket, _os);
    _reactor.removeEventHandler(_socket, _or);
    _reactor.removeEventHandler(_socket, _ow);
}

std::string ServerNet::ServiceHandler::getUid() {
    return _uid;
}

void ServerNet::ServiceHandler::onReadable(Poco::Net::ReadableNotification *pNotification) {
    Poco::Net::StreamSocket socket = pNotification->socket();

    try {
        while (socket.available()) {
            Base::ByteBuffer temp(0);
            temp.setCapacity(SOCKET_BUFFER_LEN);
            if (socket.receiveBytes(temp.begin(), SOCKET_BUFFER_LEN) <= 0 || temp.empty()) {
                closeEvent.notify(this, socket);
                return;
            }
            _buffer.append(temp);
        }

        setTaskMessage(pNotification);
    }
    catch(Poco::Net::NetException& e) {
        std::cerr << "socket read exception: " << e.displayText() << std::endl;
        close();
        delete this;
    }
    catch(Base::Exception& e) {
        std::cerr << "[Server inner error] " << e.what() << std::endl;
        close();
        delete this;
    }
}

void ServerNet::ServiceHandler::onWritable(Poco::Net::WritableNotification *pNotification) {
    Poco::Net::StreamSocket socket = pNotification->socket();

    try {
        Base::Message message;
        while (_message.tryGetResultMessage(message, 100)) {
            if (message.getTypeName() == "ServerNet::CloseConn") {
                close();
                delete this;
                return;
            }
            Base::ByteBuffer temp(message.size());
            message.serialize(temp.begin(), temp.sizeBytes());
            if (socket.sendBytes(temp.begin(), static_cast<int>(temp.sizeBytes())) != static_cast<int>(temp.sizeBytes()))
                throw Base::Exception("Send message error, size = " + std::to_string(temp.sizeBytes()));
        }
    }
    catch(Poco::Net::NetException& e) {
        std::cerr << "socket read exception: " << e.displayText() << std::endl;
        close();
        delete this;
    }
    catch(Base::Exception& e) {
        std::cerr << "[Server inner error] " << e.what() << std::endl;
        close();
        delete this;
    }
}

void ServerNet::ServiceHandler::onShutdown(Poco::Net::ShutdownNotification *pNotification) {
    std::cout << "Connection " << pNotification->name() << "shutdown..." << std::endl;
    close();
    delete this;
}

void ServerNet::ServiceHandler::onTimeout(Poco::Net::TimeoutNotification *pNotification) {
    std::cout << "Connection " << pNotification->name() << " timeout, please try again later." << std::endl;
    close();
    delete this;
}

void ServerNet::ServiceHandler::onError(Poco::Net::ErrorNotification *pNotification) {
    std::cout << "Something wrong, connection " << pNotification->name() << " shutdown..." << std::endl;
    close();
    delete this;
}

ServerNet::ServiceMessage &ServerNet::ServiceHandler::getServiceMessage() {
    return _message;
}

void ServerNet::ServiceHandler::updateTimeTick() {
    Poco::Timestamp now;
    _timeTick = now;
}

void ServerNet::ServiceHandler::setTaskMessage(Poco::Net::ReadableNotification *pNotification) {
    while (true) {
        Base::MessagePtr pMessage = Base::Message::getMessage(_buffer);
        if (nullptr == pMessage)
            return;
        // 消息加入队列等待工作线程处理
        _message.pushTaskMessage(*pMessage);
    }
}

void ServerNet::ServiceHandler::close() {
    _socket.shutdown();
    _socket.close();
    _reactor.stop();
}

void ServerNet::ServiceHandler::generateUid() {
    Poco::Net::SocketAddress peerAddr = _socket.peerAddress();
    std::string clientAddr = peerAddr.host().toString() + ":" + std::to_string(peerAddr.port());
    Poco::UUIDGenerator& generator = Poco::UUIDGenerator::defaultGenerator();
    Poco::UUID uuid = generator.createFromName(Poco::UUID::dns(), clientAddr);
    _uid = uuid.toString();
}

void ServerNet::ServiceHandler::responseUid() {
    IM::BaseType::ImMsgTcpConn connResMsg;
    Poco::Timestamp now;
    connResMsg.set_time_stamp(now.epochMicroseconds());
    connResMsg.set_uid(_uid);

    int size = connResMsg.ByteSizeLong();
    unsigned char *pBuf = new unsigned char[size];
    connResMsg.SerializePartialToArray(pBuf, size);
    Base::ByteBuffer buffer(pBuf, size);
    Base::Message message(buffer, connResMsg.GetTypeName());
    _message.sendServiceResult(message);
}
