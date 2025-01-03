//
// Created by fan on 24-12-28.
//

#include "ServiceProvider.h"
#include "Poco/Net/TCPServerParams.h"
#include "Poco/Net/TCPServer.h"
#include "google/protobuf/descriptor.h"
#include "ApplicationConfig.h"

void RpcService::ServiceProvider::publishService(google::protobuf::Service *pService) {
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

void RpcService::ServiceProvider::run() {
    // TODO: 向 zookeeper 发布服务

    // TODO: 启动网络服务
    std::string ip = RpcService::ApplicationConfig::getInstance()->getConfig()->getString("server.listen_ip");
    uint16_t port = RpcService::ApplicationConfig::getInstance()->getConfig()->getUInt16("service.listen_port");
    Poco::Net::TCPServerParams* pParams = new Poco::Net::TCPServerParams;
    pParams->setMaxQueued(DEFAULT_MAX_CONN);
    pParams->setMaxThreads(DEFAULT_THREAD_NUM);
}
