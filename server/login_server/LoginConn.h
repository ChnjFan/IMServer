//
// Created by fan on 24-11-26.
//

#ifndef IMSERVER_LOGINCONN_H
#define IMSERVER_LOGINCONN_H

#include "TcpConn.h"

class LoginConn : public Common::TcpConn {
public:
    explicit LoginConn(const Poco::Net::StreamSocket& socket);

protected:
    void handleRecvMsg() override;
    void handleTcpConnError() override;
};

class LoginConnFactory : public Poco::Net::TCPServerConnectionFactory {
public:
    Poco::Net::TCPServerConnection* createConnection(const Poco::Net::StreamSocket& socket) override {
        return new LoginConn(socket);
    }
};

#endif //IMSERVER_LOGINCONN_H
