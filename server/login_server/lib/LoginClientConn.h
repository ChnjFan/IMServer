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

class LoginClientConn : public Poco::Runnable {
public:
    LoginClientConn(Poco::Net::SocketReactor& reactor, Poco::ThreadPool& threadPool);
    ~LoginClientConn();

    void run() override;

private:
    void onReadable(Poco::Net::ReadableNotification *pNotification);
    void onWritable(Poco::Net::WritableNotification *pNotification);
    void onError(Poco::Net::ErrorNotification *pNotification);

private:
    static constexpr int SOCKET_BUFFER_LEN = 1024;
    std::shared_ptr<Poco::Net::StreamSocket> pSocket;
    Poco::Net::SocketReactor& reactor;
    Poco::ThreadPool& threadPool;

    std::string serverIP;
    int serverPort;

    ByteStream recvMsgBuf;
    ByteStream sendMsgBuf;
};


#endif //IMSERVER_LOGINCLIENTCONN_H
