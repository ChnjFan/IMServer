//
// Created by fan on 24-12-28.
//

#ifndef IMSERVER_SERVICEPROVIDER_H
#define IMSERVER_SERVICEPROVIDER_H

#include <string>
#include <unordered_map>
#include "google/protobuf/service.h"
#include "Poco/ThreadPool.h"
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
    void startServiceNet(uint16_t port);

    /**
     * @brief 运行 rpc 服务，提供远程调用服务
     */

    template<class Worker>
    void run(ApplicationConfig& config) {
        std::string ip = config.getConfig()->getString("server.listen_ip");
        uint16_t port = config.getConfig()->getUInt16("server.listen_port");
        // TODO: 向 zookeeper 发布服务

        // 流量控制开关
        connectionLimiter(config);

        // 启动工作线程，遍历客户端请求
        Poco::ThreadPool threadPool;
        for (int i = 0; i < 8; ++i) {
            auto *pWorker = new Worker(this);
            threadPool.start(*pWorker);
        }

        // 启动网络服务
        startServiceNet(port);
    }

    void onClientConnected(ServerNet::ServiceHandler *pClientHandler);
    void onClientRemoved(const void* pSender, Poco::Net::StreamSocket &socket);

    std::unordered_set<ServerNet::ServiceHandler*> clients;

private:
    static void connectionLimiter(ApplicationConfig& config);

    static constexpr int DEFAULT_MAX_CONN = 100;
    static constexpr int DEFAULT_THREAD_NUM = 4;

    struct ServiceInfo {
        google::protobuf::Service *pService{};    /**@brief 服务对象 */
        std::unordered_map<std::string, const google::protobuf::MethodDescriptor*> methodMap;/**@brief 服务方法 */
    };

    std::unordered_map<std::string, ServiceInfo> serviceMap;
};

}
#endif //IMSERVER_SERVICEPROVIDER_H
