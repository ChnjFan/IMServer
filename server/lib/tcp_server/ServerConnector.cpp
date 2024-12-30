//
// Created by fan on 24-12-29.
//

#include "ServerConnector.h"
#include "Poco/Net/SocketAcceptor.h"

template<class ServiceHandler>
void TcpServerNet::ServerConnector<ServiceHandler>::registerConnector(Poco::Net::SocketReactor &reactor) {
    _pReactor = &reactor;
}

template<class ServiceHandler>
void TcpServerNet::ServerConnector<ServiceHandler>::onConnect() {
    Poco::Net::SocketAcceptor<ServiceHandler> acceptor(_socket, *_pReactor);
    Poco::Thread thread;

    thread.start(*_pReactor);
    thread.join();
}

template<class ServiceHandler>
void TcpServerNet::ServerConnector<ServiceHandler>::unregisterConnector() {
    _pReactor->stop();
}