//
// Created by fan on 24-12-29.
//

#ifndef IMSERVER_SERVICEHANDLER_H
#define IMSERVER_SERVICEHANDLER_H

#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/SocketReactor.h"
#include "Message.h"

namespace TcpServerNet {

class ServiceHandler {
public:
    ServiceHandler(Poco::Net::StreamSocket& socket, Poco::Net::SocketReactor& reactor);
    ~ServiceHandler();

    void onReadable(Poco::Net::ReadableNotification *pNotification);
    void onWritable(Poco::Net::ReadableNotification *pNotification);
    void onShutdown(Poco::Net::ReadableNotification *pNotification);
    void onTimeout(Poco::Net::ReadableNotification *pNotification);
    void onError(Poco::Net::ReadableNotification *pNotification);

private:
    void setTaskMessage(Poco::Net::ReadableNotification *pNotification);
    void close();

    static constexpr int SOCKET_BUFFER_LEN = 1024;

    Poco::Net::StreamSocket   _socket;
    Poco::Net::SocketReactor& _reactor;
    Base::ByteBuffer          _buffer;
};

}

#endif //IMSERVER_SERVICEHANDLER_H
