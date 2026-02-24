#pragma once

#ifndef SERVICE_INSTANCE_H
#define SERVICE_INSTANCE_H

#include <string>
#include <chrono>
#include <unordered_map>

struct ServiceConfig {
    std::string name;
    std::string id;
    std::string target;
    std::chrono::milliseconds timeout; // 默认调用超时（毫秒）
    int keepalive_time_s;              // 长连接保活时间（秒）
    int keepalive_timeout_s;           // 保活包超时时间（秒）
};

enum class ServiceType {
    Routing,
    User,
    Pay
};

class ServiceInstance {
private:
    ServiceConfig config;
    bool healthy;
    ServiceType type;
    int load;
    
public:
    ServiceInstance() : healthy(false), type(ServiceType::Routing), load(0) {}
    ServiceInstance(ServiceConfig config, ServiceType type) : config(config), healthy(false), type(type), load(0) {}
    ServiceInstance(ServiceConfig &&config, ServiceType type) : config(std::move(config)), healthy(false), type(type), load(0) {}
    
    bool isHealthy() const { return healthy; }
    void setHealthy(bool value) { healthy = value; }

    ServiceType getType() const { return type; }

    int getLoad() const { return load; }
    void setLoad(int value) { load = value; }

    ServiceConfig& getConfig() { return config; }
    const ServiceConfig& getConfig() const { return config; }
    std::string getServiceName() const { return config.name; }
    std::string getServiceId() const { return config.id; }
    std::string getTarget() const { return config.target; }
    std::chrono::milliseconds getTimeout() const { return config.timeout; }
    int getKeepaliveTimeS() const { return config.keepalive_time_s; }
    int getKeepaliveTimeoutS() const { return config.keepalive_timeout_s; }

};

#endif // SERVICE_INSTANCE_H