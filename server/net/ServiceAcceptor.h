//
// Created by fan on 25-1-5.
//

#ifndef IMSERVER_SERVICEACCEPTOR_H
#define IMSERVER_SERVICEACCEPTOR_H

#include "Poco/Net/ParallelSocketAcceptor.h"
#include "ServiceProvider.h"

namespace ServerNet {

/**
 * @class ServiceAcceptor
 * @brief 服务连接类
 * @note
 */
template<class ServiceHandler, class SR>
class ServiceAcceptor : public Poco::Net::ParallelSocketAcceptor<ServiceHandler, SR> {
public:
    ServiceAcceptor(Poco::Net::ServerSocket &socket, Poco::Net::SocketReactor &reactor, ServiceProvider *server)
            : Poco::Net::ParallelSocketAcceptor<ServiceHandler, SR>(socket, reactor) {
        pServer = server;
    }

protected:
    ServiceHandler *createServiceHandler(Poco::Net::StreamSocket &socket) override {
        auto *socketHandler = new ServiceHandler(socket,
                                                 *Poco::Net::ParallelSocketAcceptor<ServiceHandler, SR>::reactor());
        pServer->onClientConnected(socketHandler);
        return socketHandler;
    }

private:
    ServiceProvider *pServer;
};

}

#endif //IMSERVER_SERVICEACCEPTOR_H
