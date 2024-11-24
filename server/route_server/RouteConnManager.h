//
// Created by fan on 24-11-21.
//

#ifndef IMSERVER_ROUTECONNMANAGER_H
#define IMSERVER_ROUTECONNMANAGER_H

#include <map>
#include "RouteConn.h"

class RouteConnManager {
public:
    static RouteConnManager* getInstance();
    static void destroyInstance();

    void addConn(RouteConn *pRouteConn);
    void closeConn(RouteConn *pRouteConn);
    RouteConn* getConn(std::string uuid);

    void checkTimeStamp();

private:
    RouteConnManager() = default;

    static RouteConnManager *instance;
    std::map<std::string, RouteConn*> routeConnMap;
};


#endif //IMSERVER_ROUTECONNMANAGER_H
