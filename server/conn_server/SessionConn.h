/**
 * @file SessionConn.h
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
#include "Poco/Net/TCPServerConnectionFactory.h"
#include "Poco/Timer.h"
#include "Message.h"
#include "ByteStream.h"
#include "TcpConn.h"

typedef enum {
    CONN_IDLE = 0,
    CONN_VERIFY = 1,
    CONN_ONLINE = 2,
    CONN_OFFLINE = 3,
}ROUTE_CONN_STATE;

class SessionConn : public Base::TcpConn {
public:
    explicit SessionConn(const Poco::Net::StreamSocket& socket);

    const Poco::Timestamp getTimeStamp() const;
    void updateTimeStamp();

    std::string getSessionUID() const;

    bool isConnIdle();
    void setState(ROUTE_CONN_STATE state);

protected:
    void connect() override;
    void recv() override;
    void error() override;
    void generateSessionUID();

private:
    Poco::Timestamp timeStamp;
    Poco::UUID sessionUID;
    ROUTE_CONN_STATE state;
};

class SessionConnFactory : public Poco::Net::TCPServerConnectionFactory {
public:
    Poco::Net::TCPServerConnection* createConnection(const Poco::Net::StreamSocket& socket) override {
        return new SessionConn(socket);
    }
};

#endif //ROUTE_SERVER_ROUTECONN_H
