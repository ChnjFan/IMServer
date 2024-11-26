//
// Created by fan on 24-11-21.
//

#include "RouteConnManager.h"
#include "ClientHeartBeatHandler.h"

/**
 * 连接管理实例
 */
RouteConnManager *RouteConnManager::instance = nullptr;

RouteConnManager *RouteConnManager::getInstance() {
    if (instance == nullptr)
        instance = new RouteConnManager();
    return instance;
}

void RouteConnManager::destroyInstance() {
    delete instance;
    instance = nullptr;
}

void RouteConnManager::addConn(RouteConn *pRouteConn) {
    Common::TcpConnManager::addConn(pRouteConn->getSessionUID(), pRouteConn);
}

RouteConn *RouteConnManager::getConn(std::string uuid) {
    return dynamic_cast<RouteConn *>(RouteConnManager::getConn(uuid));
}

void RouteConnManager::closeConn(RouteConn *pRouteConn) {
    Common::TcpConnManager::closeConn(pRouteConn->getSessionUID(), pRouteConn);
}

void RouteConnManager::checkTimeStamp() {
    for (auto it = RouteConnManager::tcpConnMap.begin(); it != RouteConnManager::tcpConnMap.end(); ) {
        Poco::Timestamp timestamp;
        RouteConn *conn = dynamic_cast<RouteConn *>(it->second);

        if (timestamp - conn->getLstTimeStamp() > ClientHeartBeatHandler::HEARTBEAT_CHECK_TIME) {
            std::cout << "Session " << conn->getSessionUID() << " timeout" << std::endl;
            conn->close();
            it = RouteConnManager::tcpConnMap.erase(it);
        }
        else {
            it++;
        }
    }
}

RouteConnManager::RouteConnManager() : Common::TcpConnManager() {

}
