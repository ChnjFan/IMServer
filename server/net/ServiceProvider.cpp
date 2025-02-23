//
// Created by fan on 24-12-28.
//

#include "ServiceProvider.h"
#include "google/protobuf/descriptor.h"
#include "Poco/Delegate.h"
#include "ApplicationConfig.h"
#include "ServerConnectionLimiter.h"
#include "ServiceAcceptor.h"

ServerNet::ServiceProvider::ServiceProvider(ServerNet::ZookeeperClient &zkClient) : clientsVersion(0), zkClient(zkClient) {

}

void ServerNet::ServiceProvider::publishService(google::protobuf::Service *pService) {
    ServiceInfo serviceInfo;

    // 获取服务对象信息
    const google::protobuf::ServiceDescriptor *pServiceDesc = pService->GetDescriptor();
    std::string serviceName = pServiceDesc->name();
    int methodNum = pServiceDesc->method_count();

    // 保存服务名和服务方法
    for (int i = 0; i < methodNum; ++i) {
        const google::protobuf::MethodDescriptor* pMethodDesc = pServiceDesc->method(i);
        std::string methodName = pMethodDesc->name();
        serviceInfo.methodMap.insert({methodName, pMethodDesc});
    }

    serviceInfo.pService = pService;
    serviceMap.insert({serviceName, serviceInfo});
}

void ServerNet::ServiceProvider::subscribeService(const std::string pService) {
    std::string path = "/" + pService;
    std::string serviceAddress = zkClient.getData(path.c_str());
    if (serviceAddress.empty()) {
        // TODO：后续支持重连
        return;
    }

}

void ServerNet::ServiceProvider::startServiceNet(uint16_t port) {
    Poco::Net::ServerSocket serverSocket(port);
    Poco::Net::SocketReactor reactor;
    ServerNet::ServiceAcceptor<ServerNet::ServiceHandler, Poco::Net::SocketReactor> acceptor(serverSocket, reactor, this);
    reactor.run();
}

void ServerNet::ServiceProvider::onClientConnected(ServerNet::ServiceHandler *pClientHandler) {
    std::cout << "Client " << pClientHandler->getUid() << " connect success." << std::endl;
    clients.insert({pClientHandler->getUid(), pClientHandler});
    pClientHandler->closeEvent += Poco::delegate(this, &ServerNet::ServiceProvider::onClientRemoved);
}

void ServerNet::ServiceProvider::onClientRemoved(const void* pSender, Poco::Net::StreamSocket &socket) {
    for (auto it = clients.begin(); it != clients.end(); ) {
        ServerNet::ServiceHandler *pClient = it->second;
        if (pClient == pSender) {
            ((ServerNet::ServiceHandler *)pClient)->closeEvent += Poco::delegate(this, &ServerNet::ServiceProvider::onClientRemoved);
            clientsVersion++;
            delete pClient;
            it = clients.erase(it);
        }
        else {
            ++it;
        }
    }
}

void ServerNet::ServiceProvider::procClientRequestMsg(ServiceWorker*pWorker, ServiceWorkerCallback workerMsg) {
    uint32_t ver = clientsVersion;
    for (auto it = clients.begin(); it != clients.end(); ++it) {
        Poco::Mutex::ScopedLock lock(mutex);

        if (ver != clientsVersion) {
            break;
        }

        Base::Message taskMessage;
        if (it->second->getServiceMessage().tryGetTaskMessage(taskMessage, 500)) {
            workerMsg(pWorker, it->second, taskMessage);
        }
    }
}

void ServerNet::ServiceProvider::procClientResponseMsg(ServerNet::ServiceWorker *pWorker,
                                                       ServerNet::ServiceProvider::ServiceWorkerCallback workerMsg) {
    uint32_t ver = clientsVersion;
    for (auto it = clients.begin(); it != clients.end(); ++it) {
        Poco::Mutex::ScopedLock lock(mutex);

        if (ver != clientsVersion) {
            break;
        }

        Base::Message taskMessage;
        if (it->second->getServiceMessage().tryGetResultMessage(taskMessage, 500)) {
            workerMsg(pWorker, it->second, taskMessage);
        }
    }
}

void ServerNet::ServiceProvider::connectionLimiter(ServerNet::ApplicationConfig& config) {
    try {
        std::string connLimiter = config.getConfig()->getString("server.conn_limiter");
        if (connLimiter == "true")
            ServerNet::ServerConnectionLimiter::getInstance()->setLimiter(true);
        else
            ServerNet::ServerConnectionLimiter::getInstance()->setLimiter(false);
    }
    catch (...) {
        ServerNet::ServerConnectionLimiter::getInstance()->setLimiter(false);
    }
}

void ServerNet::ServiceProvider::registerService(std::string& ip, uint16_t port) {
    for (const auto& service : serviceMap) {
        std::string path = "/" + service.first;
        std::string data = ip + ":" + std::to_string(port);
        zkClient.create(path.c_str(), data.c_str(), data.size());
        std::cout << "Service: " << path << "register success in " << data << std::endl;
    }
}
