#include "ServiceDiscovery.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>

namespace routing {

ServiceDiscovery::ServiceDiscovery() : heartbeat_interval_(5), service_timeout_(30) {
}

ServiceDiscovery::~ServiceDiscovery() {
}

std::vector<std::shared_ptr<ServiceInstance>> ServiceDiscovery::getServiceInstances(const std::string& service_name) {
    std::shared_lock<std::shared_mutex> lock(services_mutex_);
    
    auto it = services_.find(service_name);
    if (it != services_.end()) {
        return it->second;
    }
    
    return {};
}

bool ServiceDiscovery::registerServiceInstance(const ServiceInstance& instance) {
    std::unique_lock<std::shared_mutex> lock(services_mutex_);
    
    // 检查服务实例是否已存在
    auto& instances = services_[instance.service_name];
    auto it = std::find_if(instances.begin(), instances.end(),
        [&instance](const std::shared_ptr<ServiceInstance>& existing_instance) {
            return existing_instance->service_id == instance.service_id;
        });
    
    if (it != instances.end()) {
        // 更新现有实例
        (*it)->host = instance.host;
        (*it)->port = instance.port;
        (*it)->healthy = true;
        (*it)->metadata = instance.metadata;
        return true;
    }
    
    // 添加新实例
    instances.push_back(std::make_shared<ServiceInstance>(instance));
    return true;
}

bool ServiceDiscovery::unregisterServiceInstance(const std::string& service_id) {
    std::unique_lock<std::shared_mutex> lock(services_mutex_);
    
    // 遍历所有服务
    for (auto& [service_name, instances] : services_) {
        // 查找并移除实例
        auto it = std::remove_if(instances.begin(), instances.end(),
            [&service_id](const std::shared_ptr<ServiceInstance>& instance) {
                return instance->service_id == service_id;
            });
        
        if (it != instances.end()) {
            instances.erase(it, instances.end());
            // 如果服务没有实例了，移除服务
            if (instances.empty()) {
                services_.erase(service_name);
            }
            return true;
        }
    }
    
    return false;
}

void ServiceDiscovery::heartbeat() {
    std::unique_lock<std::shared_mutex> lock(services_mutex_);
    
    // 检查所有服务实例
    for (auto& [service_name, instances] : services_) {
        // 过滤出健康的实例
        auto it = std::remove_if(instances.begin(), instances.end(),
            [this](const std::shared_ptr<ServiceInstance>& instance) {
                // 检查实例健康状态
                bool healthy = checkServiceHealth(*instance);
                instance->healthy = healthy;
                return !healthy;
            });
        
        instances.erase(it, instances.end());
        
        // 如果服务没有实例了，移除服务
        if (instances.empty()) {
            services_.erase(service_name);
        }
    }
}

bool ServiceDiscovery::checkServiceHealth(const ServiceInstance& instance) {
    // 这里实现健康检查逻辑
    // 实际实现中可能需要：
    // 1. 发送健康检查请求到服务实例
    // 2. 检查服务实例是否响应
    // 3. 检查响应时间是否在合理范围内
    
    // 暂时返回true，表示所有实例都是健康的
    // 实际实现中应该根据具体情况进行健康检查
    return true;
}

} // namespace routing
