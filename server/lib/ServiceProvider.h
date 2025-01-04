//
// Created by fan on 24-12-28.
//

#ifndef IMSERVER_SERVICEPROVIDER_H
#define IMSERVER_SERVICEPROVIDER_H

#include <string>
#include <unordered_map>
#include "google/protobuf/service.h"
#include "Poco/Net/ParallelSocketAcceptor.h"
#include "ServiceHandler.h"
#include "ApplicationConfig.h"

namespace ServerNet {

/**
 * @class ServiceProvider
 * @brief rpc 服务发布类
 * @note 服务端发布 rpc 服务，向 zookeeper 注册服务名、方法和地址
 */
class ServiceProvider {
public:
    /**
     * @brief 发布 rpc 服务
     * @param pService rpc 服务
     */
    void publishService(google::protobuf::Service *pService);
    /**
     * @brief 运行 rpc 服务，提供远程调用服务
     */
    void run(ApplicationConfig& config);

    void onClientConnected(ServerNet::ServiceHandler *pClientHandler);
    void onClientRemoved(const void* pSender, Poco::Net::StreamSocket &socket);

private:
    static void connectionLimiter(ApplicationConfig& config);

    static constexpr int DEFAULT_MAX_CONN = 100;
    static constexpr int DEFAULT_THREAD_NUM = 4;

    struct ServiceInfo {
        google::protobuf::Service *pService;    /**@brief 服务对象 */
        std::unordered_map<std::string, const google::protobuf::MethodDescriptor*> methodMap;/**@brief 服务方法 */
    };

    std::unordered_map<std::string, ServiceInfo> serviceMap;
    std::unordered_set<ServerNet::ServiceHandler*> clients;
};

template <class ServiceHandler, class SR>
class ServiceAcceptor : public Poco::Net::ParallelSocketAcceptor<ServiceHandler, SR> {
public:
    ServiceAcceptor(Poco::Net::ServerSocket& socket, Poco::Net::SocketReactor& reactor, ServiceProvider* server)
                    : Poco::Net::ParallelSocketAcceptor<ServiceHandler, SR>(socket, reactor) {
        pServer = server;
    }
protected:
    ServiceHandler* createServiceHandler(Poco::Net::StreamSocket& socket) override {
        ServiceHandler *socketHandler = new ServiceHandler(socket, *Poco::Net::ParallelSocketAcceptor<ServiceHandler, SR>::reactor());
        pServer->onClientConnected(socketHandler);
        return socketHandler;
    }
private:
    ServiceProvider *pServer;
};

}
#endif //IMSERVER_SERVICEPROVIDER_H
