//
// Created by fan on 24-11-21.
//

#ifndef IMSERVER_SESSIONCONNMANAGER_H
#define IMSERVER_SESSIONCONNMANAGER_H

#include "SessionConn.h"
#include "TcpConnManager.h"

class SessionConnManager : public Base::TcpConnManager {
public:
    static SessionConnManager *getInstance();
    void destroyInstance();

    void add(SessionConn *pRouteConn);
    void close(SessionConn *pRouteConn);
    SessionConn* get(std::string uuid);

    void checkTimeStamp();

private:
    SessionConnManager();
    static SessionConnManager *instance;
};


#endif //IMSERVER_SESSIONCONNMANAGER_H
