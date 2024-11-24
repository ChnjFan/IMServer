//
// Created by fan on 24-11-24.
//

#ifndef IMSERVER_LOGINSERVERRESULT_H
#define IMSERVER_LOGINSERVERRESULT_H

#include "ByteStream.h"
#include "IMPdu.h"
#include "LoginClientConn.h"

class LoginServerResult {
public:
    static LoginServerResult* getInstance();

    static void handleLoginResult(ByteStream& recvMsgBuf);
    void registerLoginServer(Poco::Net::SocketReactor& reactor, Poco::ThreadPool& threadPool);
    void sendPdu(IMPdu& imPdu);

private:
    LoginServerResult() = default;
    static LoginServerResult *instance;
    LoginClientConn *pConn;
};


#endif //IMSERVER_LOGINSERVERRESULT_H
