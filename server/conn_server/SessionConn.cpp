//
// Created by fan on 24-11-11.
//

#include <memory>
#include "Poco/Runnable.h"
#include "Poco/Timer.h"
#include "SessionConn.h"
#include "MsgDispatch.h"
#include "SessionConnManager.h"

SessionConn::SessionConn(const Poco::Net::StreamSocket &socket)
                    : TcpConn(socket)
                    , lstTimeStamp()
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

void SessionConn::newConnect() {
    generateSessionUID();
    SessionConnManager::getInstance()->addConn(this);
    std::cout << "Session " << sessionUID.toString() << " conn" << std::endl;
}

void SessionConn::reactorClose() {
    std::cout << "Session " << sessionUID.toString() << " close" << std::endl;
}

void SessionConn::handleTcpConnError() {
    SessionConnManager::getInstance()->closeConn(this);
}

// 分发消息
void SessionConn::handleRecvMsg() {
    while (true) {
        std::shared_ptr<Common::IMPdu> pImPdu = Common::IMPdu::readPdu(getRecvMsgBuf());
        if (pImPdu == nullptr)
            return;

        MsgDispatch::exec(*this, pImPdu);
    }
}

const Poco::Timestamp SessionConn::getLstTimeStamp() const {
    return lstTimeStamp;
}

void SessionConn::updateLsgTimeStamp() {
    lstTimeStamp.update();
}

void SessionConn::generateSessionUID() {
    Poco::UUIDGenerator uuidGen;
    sessionUID = uuidGen.create();
}



