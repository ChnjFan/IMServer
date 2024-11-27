//
// Created by fan on 24-11-26.
//

#ifndef IMSERVER_LOGINCONN_H
#define IMSERVER_LOGINCONN_H

#include "TcpConn.h"
#include "IM.BaseType.pb.h"
#include "IM.Login.pb.h"

class LoginConn : public Common::TcpConn {
public:
    explicit LoginConn(const Poco::Net::StreamSocket& socket);

    void responseLogin(Common::IMPdu& imPdu, IM::BaseType::ResultType resultType);

protected:
    void handleRecvMsg() override;
    void handleTcpConnError() override;

private:
    void handleLogin(Common::IMPdu& imPdu);
};

class LoginConnFactory : public Poco::Net::TCPServerConnectionFactory {
public:
    Poco::Net::TCPServerConnection* createConnection(const Poco::Net::StreamSocket& socket) override {
        return new LoginConn(socket);
    }
};

#endif //IMSERVER_LOGINCONN_H
