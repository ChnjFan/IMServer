//
// Created by fan on 24-11-21.
//

#include "SessionConnManager.h"
#include "HeartBeatHandler.h"

/**
 * 连接管理实例
 */
SessionConnManager *SessionConnManager::instance = nullptr;

SessionConnManager *SessionConnManager::getInstance() {
    if (instance == nullptr)
        instance = new SessionConnManager();
    return instance;
}

void SessionConnManager::destroyInstance() {
    delete instance;
    instance = nullptr;
}

void SessionConnManager::addConn(SessionConn *pRouteConn) {
    Common::TcpConnManager::addConn(pRouteConn->getSessionUID(), pRouteConn);
}

SessionConn *SessionConnManager::getConn(std::string uuid) {
    return dynamic_cast<SessionConn *>(SessionConnManager::getConn(uuid));
}

void SessionConnManager::closeConn(SessionConn *pRouteConn) {
    Common::TcpConnManager::closeConn(pRouteConn->getSessionUID(), pRouteConn);
}

void SessionConnManager::checkTimeStamp() {
    for (auto it = SessionConnManager::tcpConnMap.begin(); it != SessionConnManager::tcpConnMap.end(); ) {
        Poco::Timestamp timestamp;
        SessionConn *conn = dynamic_cast<SessionConn *>(it->second);

        if (timestamp - conn->getLstTimeStamp() > HeartBeatHandler::HEARTBEAT_CHECK_TIME) {
            std::cout << "Session " << conn->getSessionUID() << " timeout" << std::endl;
            //TODO: 向 account_server 发送用户下线消息
            conn->close();
            it = SessionConnManager::tcpConnMap.erase(it);
        }
        else {
            it++;
        }
    }
}

SessionConnManager::SessionConnManager() : Common::TcpConnManager() {

}
