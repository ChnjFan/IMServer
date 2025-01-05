//
// Created by fan on 24-12-29.
//

#include "ServiceHandler.h"
#include "Poco/Net/NetException.h"
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
}

ServerNet::ServiceHandler::~ServiceHandler() {
    _reactor.removeEventHandler(_socket, _ot);
    _reactor.removeEventHandler(_socket, _oe);
    _reactor.removeEventHandler(_socket, _os);
    _reactor.removeEventHandler(_socket, _or);
    _reactor.removeEventHandler(_socket, _ow);
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
