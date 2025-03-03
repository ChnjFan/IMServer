//
// Created by fan on 25-2-25.
//

#include "MessageHandler.h"
#include "Poco/Net/NetException.h"
#include "Exception.h"
#include "Message.h"
#include "ImServer.h"

MessageHandler::MessageHandler(Poco::Net::StreamSocket &socket, Poco::Net::SocketReactor &reactor)
        : socket(socket)
        , reactor(reactor)
        , buffer()
        , readObserver(*this, &MessageHandler::onReadable)
        , writeObserver(*this, &MessageHandler::onWritable)
        , shutdownObserver(*this, &MessageHandler::onShutdown)
{
    reactor.addEventHandler(socket, readObserver);
    reactor.addEventHandler(socket, writeObserver);
    reactor.addEventHandler(socket, shutdownObserver);
    onConnection();
}

void MessageHandler::onConnection() {
    std::cout << "[Client] New client: " << socket.peerAddress().toString() << " connect success." << std::endl;
}

void MessageHandler::onReadable(Poco::Net::ReadableNotification *pNotification) {
    try {
        while (socket.available()) {
            char buf[Base::Buffer::DEFAULT_BUFFER_SIZE] = {0};
            if (socket.receiveBytes(buf, Base::Buffer::DEFAULT_BUFFER_SIZE) <= 0) {
                return;
            }

            buffer.append(buf, Base::Buffer::DEFAULT_BUFFER_SIZE);
            Base::MessagePtr messagePtr = Base::Message::parseMessage(buffer);
            if (nullptr != messagePtr) {
                ServerHandle::getInstance().setTask(messagePtr);
            }
        }
    }
    catch(Poco::Net::NetException& e) {
        std::cerr << "socket read exception: " << e.displayText() << std::endl;
        delete this;
    }
    catch(Base::Exception& e) {
        std::cerr << "[Server inner error] " << e.what() << std::endl;
        delete this;
    }
}

void MessageHandler::onWritable(Poco::Net::WritableNotification *pNotification) {
    try {
        Base::MessagePtr message;
        // 从响应队列中获取消息发送给客户端
        while ((message = ServerHandle::getInstance().getResponse(500)) != nullptr) {
            Base::Buffer sendBuffer = Base::Message::serializeMessage(message);
            assert(sendBuffer.readableBytes() != 0);
            socket.sendBytes(sendBuffer.peek(), (int)sendBuffer.readableBytes());
        }
    }
    catch(Poco::Net::NetException& e) {
        std::cerr << "socket read exception: " << e.displayText() << std::endl;
        delete this;
    }
    catch(Base::Exception& e) {
        std::cerr << "[Server inner error] " << e.what() << std::endl;
        delete this;
    }
}

void MessageHandler::onShutdown(Poco::Net::ShutdownNotification *pNotification) {

}
