//
// Created by fan on 24-12-28.
//

#ifndef IMSERVER_SERVICEPROVIDER_H
#define IMSERVER_SERVICEPROVIDER_H

#include <string>
#include <unordered_map>
#include "google/protobuf/service.h"

namespace RpcService {

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
    void run();

private:
    static constexpr int DEFAULT_MAX_CONN = 100;
    static constexpr int DEFAULT_THREAD_NUM = 16;

    struct ServiceInfo {
        google::protobuf::Service *pService;    /**@brief 服务对象 */
        std::unordered_map<std::string, const google::protobuf::MethodDescriptor*> methodMap;/**@brief 服务方法 */
    };

    std::unordered_map<std::string, ServiceInfo> serviceMap;
};

}
#endif //IMSERVER_SERVICEPROVIDER_H
