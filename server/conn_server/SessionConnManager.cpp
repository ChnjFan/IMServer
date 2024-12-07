//
// Created by fan on 24-11-21.
//

#include "SessionConnManager.h"
#include "HeartBeatHandler.h"

/**
 * 连接管理实例
 */
SessionConnManager *SessionConnManager::instance = nullptr;
Poco::Mutex SessionConnManager::mutex;

SessionConnManager *SessionConnManager::getInstance() {
    if (instance == nullptr) {
        Poco::Mutex::ScopedLock lock(mutex);
        if (instance == nullptr){
            instance = new SessionConnManager();
        }
    }
    return instance;
}

void SessionConnManager::destroyInstance() {
    Poco::Mutex::ScopedLock lock(mutex);
    delete instance;
    instance = nullptr;
}

void SessionConnManager::add(SessionConn *pRouteConn) {
    Poco::Mutex::ScopedLock lock(mutex);
    Base::TcpConnManager::add(pRouteConn->getSessionUID(), pRouteConn);
}

SessionConn *SessionConnManager::get(std::string uuid) {
    Poco::Mutex::ScopedLock lock(mutex);
    return dynamic_cast<SessionConn *>(SessionConnManager::get(uuid));
}

void SessionConnManager::close(SessionConn *pRouteConn) {
    Poco::Mutex::ScopedLock lock(mutex);
    Base::TcpConnManager::close(pRouteConn->getSessionUID(), pRouteConn);
}

void SessionConnManager::checkTimeStamp() {
    // FIXME: 如何解决线程同步问题和性能问题
    for (auto it = SessionConnManager::tcpConnMap.begin(); it != SessionConnManager::tcpConnMap.end(); ) {
        Poco::Timestamp timestamp;
        SessionConn *conn = dynamic_cast<SessionConn *>(it->second);

        if (timestamp - conn->getTimeStamp() > HeartBeatHandlerImpl::HEARTBEAT_CHECK_TIME) {
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

SessionConnManager::SessionConnManager() : Base::TcpConnManager() { }
