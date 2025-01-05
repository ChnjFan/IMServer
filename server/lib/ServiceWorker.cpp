//
// Created by fan on 25-1-5.
//

#include "ServiceWorker.h"
#include "ServiceHandler.h"

void ServerNet::ServiceWorker::run() {
    for (auto client : server->clients) {
        Base::Message taskMessage;
        if (client->getServiceMessage().tryGetTaskMessage(taskMessage, 100)) {
            request(client, taskMessage);
        }
    }
}

void ServerNet::ServiceWorker::request(ServerNet::ServiceHandler* pClient, Base::Message &taskMessage) {

}

void ServerNet::ServiceWorker::response(ServerNet::ServiceHandler* pClient, Base::Message &resMessage) {
    pClient->getServiceMessage().sendServiceResult(resMessage);
}

