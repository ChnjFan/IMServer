//
// Created by fan on 24-11-21.
//

#include "HeartBeat.h"
#include "RouteConnManager.h"


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
    routeConnSet.insert(pRouteConn);
}

void RouteConnManager::closeConn(RouteConn *pRouteConn) {
    auto it = routeConnSet.find(pRouteConn);
    if (it == routeConnSet.end())
        return;
    pRouteConn->close();
    routeConnSet.erase(pRouteConn);
}

void RouteConnManager::checkTimeStamp() {
    for (auto it = routeConnSet.begin(); it != routeConnSet.end(); ) {
        Poco::Timestamp timestamp;
        RouteConn *conn = *it;

        if (timestamp - conn->getLstTimeStamp() > HeartBeat::HEARTBEAT_CHECK_TIME) {
            conn->close();
            it = routeConnSet.erase(it);
        }
        else {
            it++;
        }
    }
}