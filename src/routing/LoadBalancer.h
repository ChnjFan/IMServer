#pragma once

#include <vector>
#include <string>
#include <memory>
#include "MessageRouter.h"

namespace routing {

/**
 * @brief 负载均衡策略枚举
 */
enum class LoadBalanceStrategy {
    ROUND_ROBIN,    // 轮询
    RANDOM,         // 随机
    LEAST_LOAD,     // 最小负载
    LEAST_CONN,     // 最少连接
    IP_HASH         // IP哈希
};

/**
 * @brief 负载均衡器类
 * 负责选择最优的服务实例
 */
class LoadBalancer {
private:
    // 负载均衡策略
    LoadBalanceStrategy strategy_;
    // 轮询计数器
    std::unordered_map<std::string, int> round_robin_counters_;
    // 读写锁
    std::shared_mutex counters_mutex_;

public:
    LoadBalancer();
    ~LoadBalancer();

    /**
     * @brief 选择服务实例
     * @param instances 服务实例列表
     * @return 选中的服务实例
     */
    std::shared_ptr<ServiceInstance> selectInstance(const std::vector<std::shared_ptr<ServiceInstance>>& instances);

    /**
     * @brief 更新服务实例状态
     * @param instance 服务实例
     * @param healthy 是否健康
     */
    void updateInstanceStatus(ServiceInstance& instance, bool healthy);

    /**
     * @brief 设置负载均衡策略
     * @param strategy 负载均衡策略
     */
    void setStrategy(LoadBalanceStrategy strategy) { strategy_ = strategy; }

    /**
     * @brief 获取当前负载均衡策略
     * @return 负载均衡策略
     */
    LoadBalanceStrategy getStrategy() const { return strategy_; }

private:
    /**
     * @brief 轮询选择
     * @param instances 服务实例列表
     * @param service_name 服务名称
     * @return 选中的服务实例
     */
    std::shared_ptr<ServiceInstance> selectRoundRobin(const std::vector<std::shared_ptr<ServiceInstance>>& instances, const std::string& service_name);

    /**
     * @brief 随机选择
     * @param instances 服务实例列表
     * @return 选中的服务实例
     */
    std::shared_ptr<ServiceInstance> selectRandom(const std::vector<std::shared_ptr<ServiceInstance>>& instances);

    /**
     * @brief 最小负载选择
     * @param instances 服务实例列表
     * @return 选中的服务实例
     */
    std::shared_ptr<ServiceInstance> selectLeastLoad(const std::vector<std::shared_ptr<ServiceInstance>>& instances);

    /**
     * @brief 最少连接选择
     * @param instances 服务实例列表
     * @return 选中的服务实例
     */
    std::shared_ptr<ServiceInstance> selectLeastConn(const std::vector<std::shared_ptr<ServiceInstance>>& instances);

    /**
     * @brief IP哈希选择
     * @param instances 服务实例列表
     * @param ip IP地址
     * @return 选中的服务实例
     */
    std::shared_ptr<ServiceInstance> selectIpHash(const std::vector<std::shared_ptr<ServiceInstance>>& instances, const std::string& ip);
};

} // namespace routing
