//
// Created by fan on 24-12-29.
//

#ifndef IMSERVER_SERVERCONNECTION_H
#define IMSERVER_SERVERCONNECTION_H

#include "Poco/Net/TCPServerConnection.h"
#include "Poco/Net/SocketReactor.h"
#include "ServiceHandler.h"
#include "ServerConnector.h"

namespace ServerNet {

class ServerConnection: public Poco::Net::TCPServerConnection
{
public:
    ServerConnection(const Poco::Net::StreamSocket& s): TCPServerConnection(s)
    {
    }

    void run()
    {
        Poco::Net::StreamSocket& ss = socket();
        Poco::Net::SocketReactor reactor;

        ServerConnector<ServiceHandler> sc(ss, reactor);
        reactor.run();
        std::cout << "exit ServerConnection thread" << std::endl;
    }
};

}

#endif //IMSERVER_SERVERCONNECTION_H
