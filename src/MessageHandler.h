//
// Created by fan on 25-2-25.
//

#ifndef IMSERVER_MESSAGEHANDLER_H
#define IMSERVER_MESSAGEHANDLER_H

#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/SocketReactor.h"

class MessageHandler {
public:
    MessageHandler(Poco::Net::StreamSocket& socket, Poco::Net::SocketReactor& reactor);

private:
    void onReadable(Poco::Net::ReadableNotification *pNotification);
    void onWritable(Poco::Net::WritableNotification *pNotification);
    void onShutdown(Poco::Net::ShutdownNotification *pNotification);

private:
    Poco::Net::StreamSocket socket;
    Poco::Net::SocketReactor& reactor;

    Poco::Observer<MessageHandler, Poco::Net::ReadableNotification> readObserver;
    Poco::Observer<MessageHandler, Poco::Net::WritableNotification> writeObserver;
    Poco::Observer<MessageHandler, Poco::Net::ShutdownNotification> shutdownObserver;
};


#endif //IMSERVER_MESSAGEHANDLER_H
