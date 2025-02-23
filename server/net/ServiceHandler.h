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

class ServiceHandler {
public:
    ServiceHandler(Poco::Net::StreamSocket& socket, Poco::Net::SocketReactor &reactor);
    ~ServiceHandler();

    std::string getUid();

    void onReadable(Poco::Net::ReadableNotification *pNotification);
    void onWritable(Poco::Net::WritableNotification *pNotification);
    void onShutdown(Poco::Net::ShutdownNotification *pNotification);
    void onTimeout(Poco::Net::TimeoutNotification *pNotification);
    void onError(Poco::Net::ErrorNotification *pNotification);

    ServiceMessage& getServiceMessage();

    Poco::BasicEvent<Poco::Net::StreamSocket> closeEvent;

    // 更新客户端心跳时间
    void updateTimeTick();

private:
    void setTaskMessage(Poco::Net::ReadableNotification *pNotification);
    void close();
    void generateUid();
    void responseUid();

    static constexpr int SOCKET_BUFFER_LEN = 1024;

    std::string               _uid;
    Poco::Net::StreamSocket   _socket;
    Poco::Net::SocketReactor& _reactor;
    Base::ByteBuffer          _buffer;
    ServiceMessage            _message;
    Poco::Timestamp           _timeTick;

    Poco::Observer<ServiceHandler, Poco::Net::ReadableNotification> _or;
    Poco::Observer<ServiceHandler, Poco::Net::WritableNotification> _ow;
    Poco::Observer<ServiceHandler, Poco::Net::ErrorNotification>    _oe;
    Poco::Observer<ServiceHandler, Poco::Net::TimeoutNotification>  _ot;
    Poco::Observer<ServiceHandler, Poco::Net::ShutdownNotification> _os;
};

}

#endif //IMSERVER_SERVICEHANDLER_H
