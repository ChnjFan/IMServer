//
// Created by fan on 25-1-5.
//

#include "ServiceWorker.h"
#include "ServiceHandler.h"

void ServerNet::ServiceWorker::run() {
    while (true) {
        server->procClientRequestMsg(this, &ServerNet::ServiceWorker::request);
        server->procClientResponseMsg(this, &ServerNet::ServiceWorker::response);
    }
}

void ServerNet::ServiceWorker::request(ServerNet::ServiceHandler* pClient, Base::Message &taskMessage) {
    std::cout << "Service request by: " << pClient->getUid() << ". Message type: " << taskMessage.getTypeName() << std::endl;
}

void ServerNet::ServiceWorker::response(ServerNet::ServiceHandler* pClient, Base::Message &resMessage) {
    pClient->getServiceMessage().sendServiceResult(resMessage);
}

