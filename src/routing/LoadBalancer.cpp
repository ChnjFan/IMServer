#include "LoadBalancer.h"
#include <iostream>
#include <random>
#include <mutex>
#include <algorithm>
#include <chrono>

namespace routing {

LoadBalancer::LoadBalancer() : strategy_(LoadBalanceStrategy::ROUND_ROBIN) {
}

LoadBalancer::~LoadBalancer() {
}

std::shared_ptr<ServiceInstance> LoadBalancer::selectInstance(const std::vector<std::shared_ptr<ServiceInstance>>& instances) {
    if (instances.empty()) {
        return nullptr;
    }

    // 过滤健康的实例
    std::vector<std::shared_ptr<ServiceInstance>> healthy_instances;
    for (const auto& instance : instances) {
        if (instance->healthy) {
            healthy_instances.push_back(instance);
        }
    }

    if (healthy_instances.empty()) {
        return nullptr;
    }

    // 根据策略选择实例
    switch (strategy_) {
        case LoadBalanceStrategy::ROUND_ROBIN:
            return selectRoundRobin(healthy_instances, healthy_instances[0]->service_name);
        case LoadBalanceStrategy::RANDOM:
            return selectRandom(healthy_instances);
        case LoadBalanceStrategy::LEAST_LOAD:
            return selectLeastLoad(healthy_instances);
        case LoadBalanceStrategy::LEAST_CONN:
            // 暂时使用最小负载策略
            return selectLeastLoad(healthy_instances);
        case LoadBalanceStrategy::IP_HASH:
            // 暂时使用随机策略
            return selectRandom(healthy_instances);
        default:
            return selectRoundRobin(healthy_instances, healthy_instances[0]->service_name);
    }
}

void LoadBalancer::updateInstanceStatus(const ServiceInstance& instance, bool healthy) {
    // 这里可以更新实例的健康状态
    // 实际实现中可能需要维护实例状态的映射
    instance.healthy = healthy;
}

std::shared_ptr<ServiceInstance> LoadBalancer::selectRoundRobin(const std::vector<std::shared_ptr<ServiceInstance>>& instances, const std::string& service_name) {
    if (instances.empty()) {
        return nullptr;
    }

    std::unique_lock<std::shared_mutex> lock(counters_mutex_);
    
    // 获取当前计数器值
    int& counter = round_robin_counters_[service_name];
    
    // 选择实例
    int index = counter % instances.size();
    auto selected = instances[index];
    
    // 更新计数器
    counter++;
    
    return selected;
}

std::shared_ptr<ServiceInstance> LoadBalancer::selectRandom(const std::vector<std::shared_ptr<ServiceInstance>>& instances) {
    if (instances.empty()) {
        return nullptr;
    }

    // 使用随机数生成器
    static std::mt19937_64 rng(std::chrono::steady_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<size_t> dist(0, instances.size() - 1);
    
    size_t index = dist(rng);
    return instances[index];
}

std::shared_ptr<ServiceInstance> LoadBalancer::selectLeastLoad(const std::vector<std::shared_ptr<ServiceInstance>>& instances) {
    if (instances.empty()) {
        return nullptr;
    }

    // 找到负载最小的实例
    auto min_instance = instances[0];
    int min_load = instances[0]->load;

    for (size_t i = 1; i < instances.size(); i++) {
        if (instances[i]->load < min_load) {
            min_load = instances[i]->load;
            min_instance = instances[i];
        }
    }

    return min_instance;
}

std::shared_ptr<ServiceInstance> LoadBalancer::selectLeastConn(const std::vector<std::shared_ptr<ServiceInstance>>& instances) {
    // 暂时使用最小负载策略
    return selectLeastLoad(instances);
}

std::shared_ptr<ServiceInstance> LoadBalancer::selectIpHash(const std::vector<std::shared_ptr<ServiceInstance>>& instances, const std::string& ip) {
    // 暂时使用随机策略
    return selectRandom(instances);
}

} // namespace routing
