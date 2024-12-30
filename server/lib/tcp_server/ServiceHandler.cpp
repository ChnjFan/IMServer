//
// Created by fan on 24-12-29.
//

#include "ServiceHandler.h"
#include "Poco/Net/NetException.h"
#include "ServiceMessage.h"
#include "Exception.h"

TcpServerNet::ServiceHandler::ServiceHandler(Poco::Net::StreamSocket &socket, Poco::Net::SocketReactor &reactor) :
        _socket(socket),
        _reactor(reactor),
        _buffer(SOCKET_BUFFER_LEN)
{
    _reactor.addEventHandler(_socket,
                             Poco::Observer<ServiceHandler, Poco::Net::ReadableNotification>(
                                     *this, reinterpret_cast<void (ServiceHandler::*)(
                                             Poco::Net::ReadableNotification *)>(&ServiceHandler::onReadable)));
    _reactor.addEventHandler(_socket,
                             Poco::Observer<ServiceHandler, Poco::Net::WritableNotification>(
                                     *this, reinterpret_cast<void (ServiceHandler::*)(
                                             Poco::Net::WritableNotification *)>(&ServiceHandler::onWritable)));
    _reactor.addEventHandler(_socket,
                             Poco::Observer<ServiceHandler, Poco::Net::ShutdownNotification>(
                                     *this, reinterpret_cast<void (ServiceHandler::*)(
                                             Poco::Net::ShutdownNotification *)>(&ServiceHandler::onShutdown)));
    _reactor.addEventHandler(_socket,
                             Poco::Observer<ServiceHandler, Poco::Net::TimeoutNotification>(
                                     *this, reinterpret_cast<void (ServiceHandler::*)(
                                             Poco::Net::TimeoutNotification *)>(&ServiceHandler::onTimeout)));
    _reactor.addEventHandler(_socket,
                             Poco::Observer<ServiceHandler, Poco::Net::ErrorNotification>(
                                     *this, reinterpret_cast<void (ServiceHandler::*)(
                                             Poco::Net::ErrorNotification *)>(&ServiceHandler::onError)));
}

TcpServerNet::ServiceHandler::~ServiceHandler() {
    _reactor.removeEventHandler(_socket,
                                Poco::Observer<ServiceHandler, Poco::Net::ErrorNotification>(
                                        *this, reinterpret_cast<void (ServiceHandler::*)(
                                                Poco::Net::ErrorNotification *)>(&ServiceHandler::onError)));
    _reactor.removeEventHandler(_socket,
                                Poco::Observer<ServiceHandler, Poco::Net::TimeoutNotification>(
                                        *this, reinterpret_cast<void (ServiceHandler::*)(
                                                Poco::Net::TimeoutNotification *)>(&ServiceHandler::onTimeout)));
    _reactor.removeEventHandler(_socket,
                                Poco::Observer<ServiceHandler, Poco::Net::ShutdownNotification>(
                                        *this, reinterpret_cast<void (ServiceHandler::*)(
                                                Poco::Net::ShutdownNotification *)>(&ServiceHandler::onShutdown)));
    _reactor.removeEventHandler(_socket,
                                Poco::Observer<ServiceHandler, Poco::Net::WritableNotification>(
                                        *this, reinterpret_cast<void (ServiceHandler::*)(
                                                Poco::Net::WritableNotification *)>(&ServiceHandler::onWritable)));
    _reactor.removeEventHandler(_socket,
                                Poco::Observer<ServiceHandler, Poco::Net::ReadableNotification>(
                                        *this, reinterpret_cast<void (ServiceHandler::*)(
                                                Poco::Net::ReadableNotification *)>(&ServiceHandler::onReadable)));
}

void TcpServerNet::ServiceHandler::onReadable(Poco::Net::ReadableNotification *pNotification) {
    Poco::Net::StreamSocket socket = pNotification->socket();

    try {
        while (socket.available()) {
            Base::ByteBuffer temp(SOCKET_BUFFER_LEN);
            if (socket.receiveBytes(temp.begin(), SOCKET_BUFFER_LEN) <= 0)
                break;
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

void TcpServerNet::ServiceHandler::onWritable(Poco::Net::ReadableNotification *pNotification) {
    Poco::Net::StreamSocket socket = pNotification->socket();

    try {
        Base::Message message;
        std::string connName = pNotification->name();
        while (TcpServerNet::ServiceMessage::getInstance()->getSendQueue(connName).tryPopFor(message, 100)) {
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

void TcpServerNet::ServiceHandler::onShutdown(Poco::Net::ReadableNotification *pNotification) {
    std::cout << "Connection " << pNotification->name() << "shutdown..." << std::endl;
    close();
    delete this;
}

void TcpServerNet::ServiceHandler::onTimeout(Poco::Net::ReadableNotification *pNotification) {
    std::cout << "Connection " << pNotification->name() << " timeout, please try again later." << std::endl;
    close();
    delete this;
}

void TcpServerNet::ServiceHandler::onError(Poco::Net::ReadableNotification *pNotification) {
    std::cout << "Something wrong, connection " << pNotification->name() << " shutdown..." << std::endl;
    close();
    delete this;
}

void TcpServerNet::ServiceHandler::setTaskMessage(Poco::Net::ReadableNotification *pNotification) {
    while (true) {
        Base::MessagePtr pMessage = Base::Message::getMessage(_buffer);
        if (nullptr == pMessage)
            return;
        // 消息加入队列等待工作线程处理
        TcpServerNet::ServiceMessage::getInstance()->pushTaskMessage(*pMessage, pNotification->name());
    }
}

void TcpServerNet::ServiceHandler::close() {
    _socket.shutdown();
    _socket.close();
    _reactor.stop();
}
