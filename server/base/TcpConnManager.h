//
// Created by fan on 24-11-26.
//

#ifndef IMSERVER_TCPCONNMANAGER_H
#define IMSERVER_TCPCONNMANAGER_H

#include "TcpConn.h"

namespace Base {

class TcpConnManager {
public:
    TcpConnManager() = default;

    void add(std::string key, Base::TcpConn *pRouteConn);
    void close(std::string key, Base::TcpConn *pRouteConn);
    Base::TcpConn* get(std::string key);

protected:
    std::map<std::string, Base::TcpConn*> tcpConnMap;
};

}

#endif //IMSERVER_TCPCONNMANAGER_H
