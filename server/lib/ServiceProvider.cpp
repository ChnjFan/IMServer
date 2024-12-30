//
// Created by fan on 24-12-28.
//

#include "ServiceProvider.h"
#include "Poco/Net/TCPServerParams.h"
#include "Poco/Net/TCPServer.h"
#include "Poco/Net/TCPServerConnectionFactory.h"
#include "google/protobuf/descriptor.h"
#include "ApplicationConfig.h"
#include "ServerConnection.h"
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

void ServerNet::ServiceProvider::run() {
    std::string ip = ServerNet::ApplicationConfig::getInstance()->getConfig()->getString("server.listen_ip");
    uint16_t port = ServerNet::ApplicationConfig::getInstance()->getConfig()->getUInt16("service.listen_port");
    // TODO: 向 zookeeper 发布服务

    // 流量控制开关
    connectionLimiter();

    // 启动网络服务
    Poco::Net::TCPServerParams* pParams = new Poco::Net::TCPServerParams;
    pParams->setMaxQueued(DEFAULT_MAX_CONN);
    pParams->setMaxThreads(DEFAULT_THREAD_NUM);
    pParams->setThreadIdleTime(100);
    Poco::Net::ServerSocket socket(port);

    Poco::Net::TCPServer server(new Poco::Net::TCPServerConnectionFactoryImpl<TcpServerNet::ServerConnection>(), socket, pParams);
    server.start();
}

void ServerNet::ServiceProvider::connectionLimiter() {
    try {
        std::string connLimiter = ServerNet::ApplicationConfig::getInstance()->getConfig()->getString("server.conn_limiter");
        if (connLimiter == "true")
            TcpServerNet::ServerConnectionLimiter::getInstance()->setLimiter(true);
        else
            TcpServerNet::ServerConnectionLimiter::getInstance()->setLimiter(false);
    }
    catch (...) {
        TcpServerNet::ServerConnectionLimiter::getInstance()->setLimiter(false);
    }
}
