//
// Created by fan on 24-11-11.
//

#include <memory>
#include "Poco/Runnable.h"
#include "Poco/Timer.h"
#include "Exception.h"
#include "SessionConn.h"
#include "MsgDispatcher.h"
#include "SessionConnManager.h"

SessionConn::SessionConn(const Poco::Net::StreamSocket &socket)
                    : TcpConn(socket)
                    , timeStamp()
                    , sessionUID()
                    , state(ROUTE_CONN_STATE::CONN_IDLE) { }

std::string SessionConn::getSessionUID() const {
    return sessionUID.toString();
}

bool SessionConn::isConnIdle() {
    return state == ROUTE_CONN_STATE::CONN_IDLE;
}

void SessionConn::setState(ROUTE_CONN_STATE state) {
    this->state = state;
}

void SessionConn::connect() {
    generateSessionUID();
    SessionConnManager::getInstance()->add(this);
    std::cout << "Session " << sessionUID.toString() << " conn" << std::endl;
}

// 分发消息
void SessionConn::recv() {
    try {
        while (true) {
            Base::MessagePtr pMessage = Base::Message::getMessage(getRecvMsgBuf());
            if (pMessage == nullptr)
                return;

            MsgDispatcher::exec(*this, pMessage);
        }
    }
    catch (const Base::Exception& e) {
        std::cerr << "Get Message error: " << e.what() << std::endl;
        // 消息异常后关闭客户端连接，需要客户端重连重试
        close();
    }
}

void SessionConn::error() {
    SessionConnManager::getInstance()->close(this);
}

const Poco::Timestamp SessionConn::getTimeStamp() const {
    return timeStamp;
}

void SessionConn::updateTimeStamp() {
    timeStamp.update();
}

void SessionConn::generateSessionUID() {
    Poco::UUIDGenerator uuidGen;
    sessionUID = uuidGen.create();
}



