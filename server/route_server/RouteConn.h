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
#include "Poco/Util/ServerApplication.h"
#include "Poco/Net/TCPServer.h"
#include "Poco/Net/TCPServerParams.h"
#include "Poco/Net/TCPServerConnectionFactory.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/SocketNotification.h"
#include "Poco/Timer.h"
#include "IMPdu.h"
#include "ByteStream.h"


class RouteConn : public Poco::Net::TCPServerConnection {
public:
    explicit RouteConn(const Poco::Net::StreamSocket& socket);

    void run() override;
    void close();

    void send(char* msg, uint32_t len);
    void sendPdu(IMPdu &imPdu);

    const Poco::Timestamp getLstTimeStamp() const;
    void updateLsgTimeStamp();

private:
    void onReadable(Poco::Net::ReadableNotification *pNotification);
    void onWritable(Poco::Net::WritableNotification *pNotification);
    void onError(Poco::Net::ErrorNotification *pNotification);
    void recvMsgHandler();

private:
    static constexpr int SOCKET_BUFFER_LEN = 1024;
    ByteStream recvMsgBuf;
    ByteStream sendMsgBuf;
    Poco::Net::StreamSocket connSocket;
    Poco::Timestamp lstTimeStamp;
};

class RouteConnFactory : public Poco::Net::TCPServerConnectionFactory {
public:
    Poco::Net::TCPServerConnection* createConnection(const Poco::Net::StreamSocket& socket) override {
        return new RouteConn(socket);
    }
};

#endif //ROUTE_SERVER_ROUTECONN_H
