//
// Created by fan on 25-2-25.
//

#ifndef IMSERVER_MESSAGESERVICE_H
#define IMSERVER_MESSAGESERVICE_H

#include <string>
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/SocketReactor.h"
#include "Poco/Net/SocketAcceptor.h"
#include "MessageHandler.h"

class MessageService {
public:
    explicit MessageService(unsigned short port);
    void start();

private:
    Poco::Net::ServerSocket socket;
    Poco::Net::SocketReactor reactor;
    Poco::Net::SocketAcceptor<MessageHandler> acceptor;
};


#endif //IMSERVER_MESSAGESERVICE_H
