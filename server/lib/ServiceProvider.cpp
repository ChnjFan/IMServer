//
// Created by fan on 24-12-28.
//

#include "ServiceProvider.h"
#include "google/protobuf/descriptor.h"
#include "Poco/Delegate.h"
#include "Poco/BasicEvent.h"
#include "ApplicationConfig.h"
#include "ServerConnectionLimiter.h"

void ServerNet::ServiceProvider::publishService(google::protobuf::Service *pService) {
    ServiceInfo serviceInfo;

    // 获取服务对象信息
    const google::protobuf::ServiceDescriptor *pServiceDesc = pService->GetDescriptor();
    std::string serviceName = pServiceDesc->name();
    int methodNum = pServiceDesc->method_count();

    std::cout << "Publish service: " << serviceName << std::endl;

    // 保存服务名和服务方法
    for (int i = 0; i < methodNum; ++i) {
        const google::protobuf::MethodDescriptor* pMethodDesc = pServiceDesc->method(i);
        std::string methodName = pMethodDesc->name();
        serviceInfo.methodMap.insert({methodName, pMethodDesc});
    }

    serviceInfo.pService = pService;
    serviceMap.insert({serviceName, serviceInfo});
}

void ServerNet::ServiceProvider::run(ServerNet::ApplicationConfig& config) {
    std::string ip = config.getConfig()->getString("server.listen_ip");
    uint16_t port = config.getConfig()->getUInt16("server.listen_port");
    // TODO: 向 zookeeper 发布服务

    // 流量控制开关
    connectionLimiter(config);

    // TODO:启动工作线程，遍历客户端请求

    // 启动网络服务
    Poco::Net::ServerSocket serverSocket(port);
    Poco::Net::SocketReactor reactor;
    ServerNet::ServiceAcceptor<ServerNet::ServiceHandler, Poco::Net::SocketReactor> acceptor(serverSocket, reactor, this);
    reactor.run();
}

void ServerNet::ServiceProvider::onClientConnected(ServerNet::ServiceHandler *pClientHandler) {
    clients.insert(pClientHandler);
    pClientHandler->closeEvent += Poco::delegate(this, &ServerNet::ServiceProvider::onClientRemoved);
}

void ServerNet::ServiceProvider::onClientRemoved(const void* pSender, Poco::Net::StreamSocket &socket) {
    for (auto it = clients.begin(); it != clients.end(); ) {
        ServerNet::ServiceHandler *pClient = *it;
        if (pClient == pSender) {
            ((ServerNet::ServiceHandler *)pClient)->closeEvent += Poco::delegate(this, &ServerNet::ServiceProvider::onClientRemoved);
            delete pClient;
            it = clients.erase(it);
        }
        else {
            ++it;
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
