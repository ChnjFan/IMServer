//
// Created by fan on 24-11-11.
//

#include <memory>
#include "Poco/Runnable.h"
#include "Poco/Timer.h"
#include "RouteConn.h"
#include "MsgHandler.h"
#include "RouteConnManager.h"

RouteConn::RouteConn(const Poco::Net::StreamSocket &socket)
                    : TcpConn(socket)
                    , lstTimeStamp()
                    , sessionUID()
                    , state(ROUTE_CONN_STATE::CONN_IDLE) { }

std::string RouteConn::getSessionUID() const {
    return sessionUID.toString();
}

bool RouteConn::isConnIdle() {
    return state == ROUTE_CONN_STATE::CONN_IDLE;
}

void RouteConn::setState(ROUTE_CONN_STATE state) {
    this->state = state;
}

void RouteConn::newConnect() {
    generateSessionUID();
    RouteConnManager::getInstance()->addConn(this);
    std::cout << "Session " << sessionUID.toString() << " conn" << std::endl;
}

void RouteConn::reactorClose() {
    std::cout << "Session " << sessionUID.toString() << " close" << std::endl;
}

void RouteConn::handleTcpConnError() {
    RouteConnManager::getInstance()->closeConn(this);
}

// 分发消息
void RouteConn::handleRecvMsg() {
    while (true) {
        std::shared_ptr<Common::IMPdu> pImPdu = Common::IMPdu::readPdu(getRecvMsgBuf());
        if (pImPdu == nullptr)
            return;

        MsgHandler::exec(*this, pImPdu);
    }
}

const Poco::Timestamp RouteConn::getLstTimeStamp() const {
    return lstTimeStamp;
}

void RouteConn::updateLsgTimeStamp() {
    lstTimeStamp.update();
}

void RouteConn::generateSessionUID() {
    Poco::UUIDGenerator uuidGen;
    sessionUID = uuidGen.create();
}



