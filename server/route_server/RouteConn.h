/**
 * @file RouteConn.h
 * @brief route_server 链接处理
 * @note route_server 接受登录消息，登录成功后发送给客户端负载最小的 msg_server 的 ip 和 port
 */

#ifndef ROUTE_SERVER_ROUTECONN_H
#define ROUTE_SERVER_ROUTECONN_H

#include <iostream>
#include <string>
#include <set>
#include "Poco/Timestamp.h"
#include "Poco/UUID.h"
#include "Poco/UUIDGenerator.h"
#include "Poco/Util/ServerApplication.h"
#include "Poco/Net/TCPServer.h"
#include "Poco/Net/TCPServerParams.h"
#include "Poco/Net/TCPServerConnectionFactory.h"
#include "Poco/Net/SocketReactor.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/SocketNotification.h"
#include "Poco/Timer.h"
#include "IMPdu.h"
#include "ByteStream.h"
#include "TcpConn.h"

typedef enum {
    CONN_IDLE = 0,
    CONN_VERIFY = 1,
    CONN_ONLINE = 2,
    CONN_OFFLINE = 3,
}ROUTE_CONN_STATE;

class RouteConn : public TcpConn {
public:
    explicit RouteConn(const Poco::Net::StreamSocket& socket);

    void sendPdu(IMPdu &imPdu);

    const Poco::Timestamp getLstTimeStamp() const;
    void updateLsgTimeStamp();

    std::string getSessionUID() const;

    bool isConnIdle();
    void setState(ROUTE_CONN_STATE state);

protected:
    void newConnect() override;
    void reactorClose() override;
    void handleRecvMsg() override;
    void handleTcpConnError() override;
    void generateSessionUID();

private:
    Poco::Timestamp lstTimeStamp;
    Poco::UUID sessionUID;
    ROUTE_CONN_STATE state;
};

class RouteConnFactory : public Poco::Net::TCPServerConnectionFactory {
public:
    Poco::Net::TCPServerConnection* createConnection(const Poco::Net::StreamSocket& socket) override {
        return new RouteConn(socket);
    }
};

#endif //ROUTE_SERVER_ROUTECONN_H
