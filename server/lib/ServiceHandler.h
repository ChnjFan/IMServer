//
// Created by fan on 24-12-29.
//

#ifndef IMSERVER_SERVICEHANDLER_H
#define IMSERVER_SERVICEHANDLER_H

#include "Poco/BasicEvent.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/SocketReactor.h"
#include "ServiceMessage.h"

namespace ServerNet {

class ServiceProvider;

class ServiceHandler {
public:
    ServiceHandler(Poco::Net::StreamSocket& socket, Poco::Net::SocketReactor &reactor);
    ~ServiceHandler();

    void onReadable(Poco::Net::ReadableNotification *pNotification);
    void onWritable(Poco::Net::WritableNotification *pNotification);
    void onShutdown(Poco::Net::ShutdownNotification *pNotification);
    void onTimeout(Poco::Net::TimeoutNotification *pNotification);
    void onError(Poco::Net::ErrorNotification *pNotification);

    Poco::BasicEvent<Poco::Net::StreamSocket> closeEvent;

private:
    void setTaskMessage(Poco::Net::ReadableNotification *pNotification);
    void close();

    static constexpr int SOCKET_BUFFER_LEN = 1024;

    Poco::Net::StreamSocket   _socket;
    Poco::Net::SocketReactor& _reactor;
    Base::ByteBuffer          _buffer;
    ServiceMessage            _message;

    Poco::Observer<ServiceHandler, Poco::Net::ReadableNotification> _or;
    Poco::Observer<ServiceHandler, Poco::Net::WritableNotification> _ow;
    Poco::Observer<ServiceHandler, Poco::Net::ErrorNotification>    _oe;
    Poco::Observer<ServiceHandler, Poco::Net::TimeoutNotification>  _ot;
    Poco::Observer<ServiceHandler, Poco::Net::ShutdownNotification> _os;
};

}

#endif //IMSERVER_SERVICEHANDLER_H
