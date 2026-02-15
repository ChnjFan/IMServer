#include "ServiceDiscovery.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>
#include <grpc++/grpc++.h>

#include "common_messages.pb.h"

#include "ServiceFactory.h"

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
    auto& instances = services_[instance.getServiceName()];
    auto it = std::find_if(instances.begin(), instances.end(),
        [&instance](const std::shared_ptr<ServiceInstance>& existing_instance) {
            return existing_instance->getServiceId() == instance.getServiceId();
        });
    
    if (it != instances.end()) {
        // todo 更新现有实例
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
                return instance->getServiceId() == service_id;
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
                instance->setHealthy(healthy);
                return !healthy;
            });
        
        instances.erase(it, instances.end());
        
        // 如果服务没有实例了，移除服务
        if (instances.empty()) {
            services_.erase(service_name);
        }
    }
}

bool ServiceDiscovery::checkServiceHealth([[maybe_unused]] const ServiceInstance& instance) {
    // 这里实现健康检查逻辑
    // 实际实现中可能需要：
    // 1. 发送健康检查请求到服务实例
    // 2. 检查服务实例是否响应
    // 3. 检查响应时间是否在合理范围内
    switch (instance.getType()) {
        case ServiceType::Routing:
            break;
        case ServiceType::User:
            return checkUserServiceHealth(instance);
            break;
        case ServiceType::Pay:
            break;
        default:
            return false;
    }
    
    // 暂时返回true，表示所有实例都是健康的
    // 实际实现中应该根据具体情况进行健康检查
    return true;
}

bool ServiceDiscovery::checkUserServiceHealth(const ServiceInstance& instance) {
    try {
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(2));
        
        google::protobuf::Empty request;
        std::unique_ptr<im::common::protocol::UserService::Stub> stub =
            ServiceFactory::GetUserServiceStub(instance.getConfig());
        if (!stub) {
            return false;
        }
        im::common::protocol::StatusResponse response;
        grpc::Status status = stub->CheckStatus(&context, request, &response);

        if (status.ok()) {
            return true;
        } else {
            std::cerr << "[ServiceDiscovery] CheckStatus failed: " 
                      << status.error_code() << " - " << status.error_message() << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "[ServiceDiscovery] Exception in CheckStatus: " << e.what() << std::endl;
        return false;
    }
}

} // namespace routing
