//
// Created by fan on 24-11-21.
//

#ifndef IMSERVER_ROUTECONNMANAGER_H
#define IMSERVER_ROUTECONNMANAGER_H

#include <map>
#include "RouteConn.h"
#include "TcpConnManager.h"

class RouteConnManager : public Common::TcpConnManager {
public:
    static RouteConnManager *getInstance();
    void destroyInstance();

    void addConn(RouteConn *pRouteConn);
    void closeConn(RouteConn *pRouteConn);
    RouteConn* getConn(std::string uuid);

    void checkTimeStamp();

private:
    RouteConnManager();
    static RouteConnManager *instance;
};


#endif //IMSERVER_ROUTECONNMANAGER_H
