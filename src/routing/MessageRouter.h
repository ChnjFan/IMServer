#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <string>

#include "gateway_routing.pb.h"

namespace routing {

using im::common::protocol::RouteRequest;
using im::common::protocol::RouteResponse;
using im::common::protocol::StatusResponse;

/**
 * @brief 服务实例类
 * 表示一个可路由的服务实例
 */
class ServiceInstance {
public:
    std::string service_id;
    std::string service_name;
    std::string host;
    int port;
    bool healthy;
    int load;
    std::unordered_map<std::string, std::string> metadata;
    
    ServiceInstance() : port(0), healthy(false), load(0) {}
    
    ServiceInstance(std::string id, std::string name, std::string h, int p)
        : service_id(std::move(id)),
          service_name(std::move(name)),
          host(std::move(h)),
          port(p),
          healthy(true),
          load(0) {}
};

/**
 * @brief 消息路由器类
 * 负责根据消息类型和目标服务进行路由
 */
class MessageRouter {
using ServiceInstancePtr = std::shared_ptr<ServiceInstance>;
using ServiceInstanceList = std::vector<ServiceInstancePtr>;
using ServiceInstanceMap = std::unordered_map<std::string, ServiceInstanceList>;

private:
    // 服务实例映射：服务名称 -> 服务实例列表
    ServiceInstanceMap services_;
    // 读写锁，支持并发访问服务列表
    std::shared_mutex services_mutex_;
    // 服务发现模块
    class ServiceDiscovery* discovery_;
    // 负载均衡模块
    class LoadBalancer* load_balancer_;
    // 消息队列模块
    class MessageQueue* message_queue_;
    // 监控模块
    class Metrics* metrics_;
    // 启动时间
    int64_t start_time_;
    // 消息处理计数
    std::atomic<int64_t> message_count_;
    // 消息处理失败计数
    std::atomic<int64_t> message_error_count_;

public:
    MessageRouter();
    ~MessageRouter();

    /**
     * @brief 路由消息
     * @param request 路由请求
     * @param response 路由响应
     */
    void routeMessage(const RouteRequest* request, RouteResponse *response);

    /**
     * @brief 注册服务实例
     * @param instance 服务实例
     */
    void registerService(const ServiceInstance& instance);

    /**
     * @brief 移除服务实例
     * @param service_id 服务实例ID
     */
    void unregisterService(const std::string& service_id);

    /**
     * @brief 获取服务实例列表
     * @param service_name 服务名称
     * @return 服务实例列表
     */
    ServiceInstanceList getServiceInstances(const std::string& service_name);

    /**
     * @brief 检查服务健康状态
     * @return 健康状态响应
     */
    void checkStatus(StatusResponse *response);

    /**
     * @brief 获取消息处理统计
     * @return 统计信息
     */
    std::unordered_map<std::string, int64_t> getStats();

private:
    /**
     * @brief 内部路由消息处理
     * @param request 路由请求
     * @param response 路由响应
     */
    void routeMessageInternal(const RouteRequest* request, RouteResponse *response);

    void registerDefaultServices();
};

} // namespace routing
