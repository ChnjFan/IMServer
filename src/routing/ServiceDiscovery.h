#pragma once

#include <vector>
#include <string>
#include <memory>
#include "MessageRouter.h"

namespace routing {

/**
 * @brief 服务发现类
 * 负责发现和管理可用的服务实例
 */
class ServiceDiscovery {

using ServiceInstanceList = std::vector<std::shared_ptr<ServiceInstance>>;
using ServiceInstanceMap = std::unordered_map<std::string, ServiceInstanceList>;

private:
    // 服务实例映射
    ServiceInstanceMap services_;
    // 读写锁
    std::shared_mutex services_mutex_;
    // 心跳间隔（秒）
    int heartbeat_interval_;
    // 服务超时时间（秒）
    int service_timeout_;

public:
    ServiceDiscovery();
    ~ServiceDiscovery();

    /**
     * @brief 获取服务实例列表
     * @param service_name 服务名称
     * @return 服务实例列表
     */
    ServiceInstanceList getServiceInstances(const std::string& service_name);

    /**
     * @brief 注册服务实例
     * @param instance 服务实例
     * @return 是否注册成功
     */
    bool registerServiceInstance(const ServiceInstance& instance);

    /**
     * @brief 注销服务实例
     * @param service_id 服务实例ID
     * @return 是否注销成功
     */
    bool unregisterServiceInstance(const std::string& service_id);

    /**
     * @brief 心跳检测
     * 更新服务实例的健康状态
     */
    void heartbeat();

    /**
     * @brief 检测服务实例健康状态
     * @param instance 服务实例
     * @return 是否健康
     */
    bool checkServiceHealth(const ServiceInstance& instance);

    /**
     * @brief 设置心跳间隔
     * @param interval 心跳间隔（秒）
     */
    void setHeartbeatInterval(int interval) { heartbeat_interval_ = interval; }

    /**
     * @brief 设置服务超时时间
     * @param timeout 超时时间（秒）
     */
    void setServiceTimeout(int timeout) { service_timeout_ = timeout; }
};

} // namespace routing
