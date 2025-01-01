//
// Created by fan on 24-12-29.
//

#include "ServerConnector.h"
#include "Poco/Net/SocketAcceptor.h"

template<class ServerHandler>
ServerNet::ServerConnector<ServerHandler>::ServerConnector(Poco::Net::StreamSocket &ss):
        _socket(ss),
        _pReactor(nullptr)
{ }

template<class ServerHandler>
ServerNet::ServerConnector<ServerHandler>::ServerConnector(Poco::Net::StreamSocket &ss, Poco::Net::SocketReactor &reactor):
        _socket(ss),
        _pReactor(nullptr)
{
    registerConnector(reactor);
    onConnect();
}

template<class ServerHandler>
ServerNet::ServerConnector<ServerHandler>::~ServerConnector() {
    unregisterConnector();
}

template<class ServerHandler>
void ServerNet::ServerConnector<ServerHandler>::registerConnector(Poco::Net::SocketReactor &reactor) {
    _pReactor = &reactor;
}

template<class ServerHandler>
void ServerNet::ServerConnector<ServerHandler>::onConnect() {
    Poco::Net::SocketAcceptor<ServerHandler> acceptor(_socket, *_pReactor);
    Poco::Thread thread;

    thread.start(*_pReactor);
    thread.join();
}

template<class ServerHandler>
void ServerNet::ServerConnector<ServerHandler>::unregisterConnector() {
    _pReactor->stop();
}