//
// Created by fan on 24-11-11.
//

#include <memory>
#include "Poco/Runnable.h"
#include "Poco/Timer.h"
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
    while (true) {
        Base::MessagePtr pMessage = Base::Message::getMessage(getRecvMsgBuf());
        if (pMessage == nullptr)
            return;

        MsgDispatcher::exec(*this, pMessage);
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



