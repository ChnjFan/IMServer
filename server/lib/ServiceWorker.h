//
// Created by fan on 25-1-5.
//

#ifndef IMSERVER_SERVICEWORKER_H
#define IMSERVER_SERVICEWORKER_H

#include "Poco/Runnable.h"
#include "Message.h"
#include "ServiceProvider.h"

namespace ServerNet {

class ServiceWorker : public Poco::Runnable {
public:
    explicit ServiceWorker(ServiceProvider* pServer) : server(pServer) { }

    void run() override;

    virtual void request(ServerNet::ServiceHandler* pClient, Base::Message& taskMessage);
    void response(ServerNet::ServiceHandler* pClient, Base::Message& resMessage);

private:
    ServiceProvider* server;
};

}

#endif //IMSERVER_SERVICEWORKER_H
