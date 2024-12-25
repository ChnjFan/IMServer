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
#include "ConnectionLimiter.h"

SessionConn::SessionConn(const Poco::Net::StreamSocket &socket)
                    : TcpConn(socket)
                    , timeStamp()
                    , sessionUID()
                    , state(ROUTE_CONN_STATE::CONN_IDLE)
                    , clientIP(socket.peerAddress().host().toString())
                    , authenticated(false) {
    initializeConnection();
}

SessionConn::~SessionConn() {
    if (!authenticated && state != ROUTE_CONN_STATE::CONN_OFFLINE) {
        // 如果连接未认证就断开，记录认证失败
        ConnectionLimiter::getInstance()->recordAuthFailure(clientIP);
    }
}

void SessionConn::initializeConnection() {
    if (!checkConnectionLimit()) {
        throw Base::Exception("Connection limit exceeded for IP: " + clientIP);
    }
    ConnectionLimiter::getInstance()->recordConnection(clientIP);
}

bool SessionConn::checkConnectionLimit() {
    return ConnectionLimiter::getInstance()->isIPAllowed(clientIP);
}

std::string SessionConn::getClientIP() const {
    return clientIP;
}

bool SessionConn::authenticate(const std::string& token) {
    if (authenticated) {
        return true;
    }

    // TODO: 实现具体的token验证逻辑
    // 这里应该调用account_server进行token验证
    bool validToken = true; // 临时设置为true，实际应该验证token

    if (validToken) {
        authenticated = true;
        setState(ROUTE_CONN_STATE::CONN_ONLINE);
        return true;
    }

    ConnectionLimiter::getInstance()->recordAuthFailure(clientIP);
    return false;
}

bool SessionConn::isAuthenticated() const {
    return authenticated;
}

void SessionConn::connect() {
    generateSessionUID();
    
    // TODO:启动定时器检查认证超时

    SessionConnManager::getInstance()->add(this);
    std::cout << "Session " << sessionUID.toString() << " from " << clientIP << " connected" << std::endl;
}

std::string SessionConn::getSessionUID() const {
    return sessionUID.toString();
}

bool SessionConn::isConnIdle() {
    return state == ROUTE_CONN_STATE::CONN_IDLE;
}

void SessionConn::setState(ROUTE_CONN_STATE state) {
    this->state = state;
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
