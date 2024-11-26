//
// Created by fan on 24-11-26.
//

#ifndef IMSERVER_TCPCONNMANAGER_H
#define IMSERVER_TCPCONNMANAGER_H

#include "TcpConn.h"

namespace Common {

class TcpConnManager {
public:
    TcpConnManager() = default;

    void addConn(std::string key, Common::TcpConn *pRouteConn);
    void closeConn(std::string key, Common::TcpConn *pRouteConn);
    Common::TcpConn* getConn(std::string key);
protected:
    std::map<std::string, Common::TcpConn*> tcpConnMap;
};

}

#endif //IMSERVER_TCPCONNMANAGER_H
