//
// Created by fan on 24-11-25.
//

#ifndef IMSERVER_TCPCONN_H
#define IMSERVER_TCPCONN_H

#include "Poco/Net/TCPServer.h"
#include "Poco/Net/TCPServerParams.h"
#include "Poco/Net/TCPServerConnectionFactory.h"
#include "Poco/Net/SocketReactor.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/SocketNotification.h"
#include "ByteStream.h"
#include "IMPdu.h"

namespace Base {

class TcpConn : public Poco::Net::TCPServerConnection {
public:
    explicit TcpConn(const Poco::Net::StreamSocket& socket);

    void run() override;
    void close();

    void send(char* msg, uint32_t len);
    void sendPdu(Base::IMPdu &imPdu);

protected:
    virtual void newConnect();
    virtual void reactorClose();
    virtual void handleRecvMsg();
    virtual void handleTcpConnError();

    void onReadable(Poco::Net::ReadableNotification *pNotification);
    void onWritable(Poco::Net::WritableNotification *pNotification);
    void onError(Poco::Net::ErrorNotification *pNotification);

    Base::ByteStream& getRecvMsgBuf();
    Base::ByteStream& getSendMsgBuf();

private:
    static constexpr int SOCKET_BUFFER_LEN = 1024;
    Base::ByteStream recvMsgBuf;
    Base::ByteStream sendMsgBuf;

    Poco::Net::StreamSocket connSocket;
    Poco::Net::SocketReactor reactor;
};

}


#endif //IMSERVER_TCPCONN_H
