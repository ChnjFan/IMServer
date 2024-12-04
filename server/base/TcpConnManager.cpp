//
// Created by fan on 24-11-26.
//

#include "TcpConnManager.h"

void Base::TcpConnManager::add(std::string key, Base::TcpConn *pRouteConn) {
    tcpConnMap[key] = pRouteConn;
}

void Base::TcpConnManager::close(std::string key, Base::TcpConn *pRouteConn) {
    auto it = tcpConnMap.find(key);
    if (it == tcpConnMap.end())
        return;
    pRouteConn->close();
    tcpConnMap.erase(it);
}

Base::TcpConn *Base::TcpConnManager::get(std::string key) {
    auto it = tcpConnMap.find(key);
    if (it == tcpConnMap.end())
        return nullptr;
    return it->second;
}
