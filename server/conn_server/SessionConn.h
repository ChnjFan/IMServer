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
#include "Poco/Net/IPAddress.h"
#include "Message.h"
#include "ByteStream.h"
#include "TcpConn.h"
#include "ConnectionLimiter.h"

typedef enum {
    CONN_IDLE = 0,
    CONN_VERIFY = 1,
    CONN_ONLINE = 2,
    CONN_OFFLINE = 3,
}ROUTE_CONN_STATE;

class SessionConn : public Base::TcpConn {
public:
    explicit SessionConn(const Poco::Net::StreamSocket& socket);
    ~SessionConn();

    const Poco::Timestamp getTimeStamp() const;
    void updateTimeStamp();

//    void SessionConn::checkAuthTimeout(Poco::Timer& timer);

    std::string getSessionUID() const;
    std::string getClientIP() const;

    bool isConnIdle();
    void setState(ROUTE_CONN_STATE state);
    bool authenticate(const std::string& token);
    bool isAuthenticated() const;

protected:
    void connect() override;
    void recv() override;
    void error() override;

private:
    void generateSessionUID();
    void initializeConnection();
    bool checkConnectionLimit();

private:
    Poco::Timestamp timeStamp;
    Poco::UUID sessionUID;

    /**
     * @brief 连接状态
     */
    ROUTE_CONN_STATE state;

    /**
     * @brief 客户端连接IP
     */
    std::string clientIP;

    /**
     * @brief 连接认证参数
     */
    bool authenticated;
    static constexpr int AUTH_TIMEOUT = 30000; // 30秒认证超时
};

class SessionConnFactory : public Poco::Net::TCPServerConnectionFactory {
public:
    Poco::Net::TCPServerConnection* createConnection(const Poco::Net::StreamSocket& socket) override {
        return new SessionConn(socket);
    }
};

#endif //ROUTE_SERVER_ROUTECONN_H
