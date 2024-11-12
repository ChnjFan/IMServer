//
// Created by fan on 24-11-11.
//
#ifndef ROUTE_SERVER_ROUTECONN_H
#define ROUTE_SERVER_ROUTECONN_H

#include "Poco/Util/ServerApplication.h"
#include "Poco/Net/TCPServer.h"
#include "Poco/Net/TCPServerParams.h"
#include "Poco/Net/TCPServerConnectionFactory.h"
#include "Poco/Net/StreamSocket.h"
#include "Poco/Timer.h"
#include <iostream>
#include <string>

class RouteConn : public Poco::Net::TCPServerConnection {
public:
    explicit RouteConn(const Poco::Net::StreamSocket& socket) : Poco::Net::TCPServerConnection(socket) {}

    void run() override {
        try {
            char buffer[256];
            int len = socket().receiveBytes(buffer, sizeof(buffer));
            if (len > 0) {
                std::cout << "Received: " << std::string(buffer, len) << std::endl;
                // Echo the received data back to the client
                socket().sendBytes(buffer, len);
            }
        } catch (Poco::Exception& e) {
            std::cerr << "Error: " << e.displayText() << std::endl;
        }
    }
};

class RouteConnFactory : public Poco::Net::TCPServerConnectionFactory {
public:
    Poco::Net::TCPServerConnection* createConnection(const Poco::Net::StreamSocket& socket) override {
        return new RouteConn(socket);
    }
};


#endif //ROUTE_SERVER_ROUTECONN_H
