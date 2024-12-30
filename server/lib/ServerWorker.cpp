//
// Created by fan on 24-12-29.
//

#include "ServerWorker.h"
#include "Exception.h"
#include "ServiceMessage.h"

void ServerNet::ServerWorker::run() {
    try {
        while (true) {
            Base::Message message;
            std::string connName;
            // 从连接消息队列中获取请求消息处理
            if (TcpServerNet::ServiceMessage::getInstance()->tryGetTaskMessage(message, connName)) {
                work(message, connName);
            }
        }
    }
    catch (Base::Exception& e) {
        std::cerr << "[ServerWorker inner error] " << e.what() << std::endl;
    }
}

void ServerNet::ServerWorker::send(std::string connName, Base::Message &message) {
    TcpServerNet::ServiceMessage::getInstance()->getSendQueue(connName).push(message);
}

void ServerNet::ServerWorker::close(std::string connName) {
    Base::ByteBuffer buffer(0);
    Base::Message message(buffer, "ServerNet::CloseConn");
    TcpServerNet::ServiceMessage::getInstance()->getSendQueue(connName).push(message);
}

// 具体工作类实现
void ServerNet::ServerWorker::work(Base::Message& message, std::string& connName) {
    std::cout << connName << message.size() << std::endl;
}
