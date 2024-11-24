//
// Created by fan on 24-11-24.
//

#ifndef IMSERVER_LOGINCLIENTCONN_H
#define IMSERVER_LOGINCLIENTCONN_H

#include <memory>
#include "Poco/ThreadPool.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/SocketReactor.h"
#include "ByteStream.h"
#include "IMPdu.h"

class LoginClientConn : public Poco::Runnable {
public:
    LoginClientConn(Poco::Net::SocketReactor& reactor, const std::function<void(ByteStream&)>& readCallback);
    ~LoginClientConn();

    void run() override;
    void sendPdu(IMPdu& imPdu);

private:
    void onReadable(Poco::Net::ReadableNotification *pNotification);
    void onWritable(Poco::Net::WritableNotification *pNotification);
    void onError(Poco::Net::ErrorNotification *pNotification);
    void connect();
    void reconnect();

private:
    static constexpr int SOCKET_BUFFER_LEN = 1024;

    std::shared_ptr<Poco::Net::StreamSocket> pSocket;
    Poco::Net::SocketReactor& reactor;

    std::string serverIP;
    int serverPort;
    bool connected;

    ByteStream recvMsgBuf;
    ByteStream sendMsgBuf;

    const std::function<void(ByteStream&)>& readCallback;
};


#endif //IMSERVER_LOGINCLIENTCONN_H
