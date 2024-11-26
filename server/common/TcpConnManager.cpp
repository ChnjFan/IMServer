//
// Created by fan on 24-11-26.
//

#include "TcpConnManager.h"

void Common::TcpConnManager::addConn(std::string key, Common::TcpConn *pRouteConn) {
    tcpConnMap[key] = pRouteConn;
}

void Common::TcpConnManager::closeConn(std::string key, Common::TcpConn *pRouteConn) {
    auto it = tcpConnMap.find(key);
    if (it == tcpConnMap.end())
        return;
    pRouteConn->close();
    tcpConnMap.erase(it);
}

Common::TcpConn *Common::TcpConnManager::getConn(std::string key) {
    auto it = tcpConnMap.find(key);
    if (it == tcpConnMap.end())
        return nullptr;
    return it->second;
}
