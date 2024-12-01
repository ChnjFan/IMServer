//
// Created by fan on 24-11-21.
//

#ifndef IMSERVER_SESSIONCONNMANAGER_H
#define IMSERVER_SESSIONCONNMANAGER_H

#include <map>
#include "SessionConn.h"
#include "TcpConnManager.h"

class SessionConnManager : public Common::TcpConnManager {
public:
    static SessionConnManager *getInstance();
    void destroyInstance();

    void addConn(SessionConn *pRouteConn);
    void closeConn(SessionConn *pRouteConn);
    SessionConn* getConn(std::string uuid);

    void checkTimeStamp();

private:
    SessionConnManager();
    static SessionConnManager *instance;
};


#endif //IMSERVER_SESSIONCONNMANAGER_H
