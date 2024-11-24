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
    routeConnMap[pRouteConn->getSessionUID()] = pRouteConn;
}

RouteConn *RouteConnManager::getConn(std::string uuid) {
    auto it = routeConnMap.find(uuid);
    if (it == routeConnMap.end())
        return nullptr;
    return it->second;
}

void RouteConnManager::closeConn(RouteConn *pRouteConn) {
    auto it = routeConnMap.find(pRouteConn->getSessionUID());
    if (it == routeConnMap.end())
        return;
    pRouteConn->close();
    routeConnMap.erase(it);
}

void RouteConnManager::checkTimeStamp() {
    for (auto it = routeConnMap.begin(); it != routeConnMap.end(); ) {
        Poco::Timestamp timestamp;
        RouteConn *conn = it->second;

        if (timestamp - conn->getLstTimeStamp() > ClientHeartBeatHandler::HEARTBEAT_CHECK_TIME) {
            std::cout << "Session " << conn->getSessionUID() << " timeout" << std::endl;
            conn->close();
            it = routeConnMap.erase(it);
        }
        else {
            it++;
        }
    }
}
