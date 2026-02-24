#include "MessageRouter.h"

#include <iostream>
#include <chrono>

#include "ServiceDiscovery.h"
#include "LoadBalancer.h"
#include "MessageQueue.h"
#include "Metrics.h"
#include "ServiceFactory.h"

namespace routing {

MessageRouter::MessageRouter() 
    : discovery_(new ServiceDiscovery()),
      load_balancer_(new LoadBalancer()),
      message_queue_(new MessageQueue()),
      metrics_(new Metrics()),
      start_time_(std::chrono::steady_clock::now().time_since_epoch().count() / 1000),
      message_count_(0),
      message_error_count_(0),
      io_context_(std::make_shared<boost::asio::io_context>()) {
    // 启动消息队列
    message_queue_->start(4);
    
    // 注册一些默认服务实例（用于测试）
    registerDefaultServices();
    
    // 启动心跳定时器
    startHeartbeatTimer();
    
    // 启动IO上下文线程
    std::thread io_thread([this]() {
        io_context_->run();
    });
    io_thread.detach();
}

MessageRouter::~MessageRouter() {
    // 停止心跳定时器
    stopHeartbeatTimer();
    
    // 停止IO上下文
    if (io_context_) {
        io_context_->stop();
    }
    
    // 停止消息队列
    message_queue_->stop();
    
    // 释放资源
    delete discovery_;
    delete load_balancer_;
    delete message_queue_;
    delete metrics_;
}

void MessageRouter::routeMessage(const RouteRequest *request, RouteResponse *response) {
    auto start = std::chrono::steady_clock::now();
    message_count_++;
    metrics_->incrementCounter(Metrics::MESSAGE_COUNT);
    
    try {
        routeMessageInternal(request, response);
        
        // 记录处理时间
        metrics_->recordTimer(Metrics::MESSAGE_LATENCY, start);
        
        if (response->accepted()) {
            metrics_->incrementCounter(Metrics::ROUTE_COUNT);
        } else {
            metrics_->incrementCounter(Metrics::ROUTE_ERROR_COUNT);
        }
    } catch (const std::exception& e) {
        // 记录错误
        message_error_count_++;
        metrics_->incrementCounter(Metrics::MESSAGE_ERROR_COUNT);
        metrics_->incrementCounter(Metrics::ROUTE_ERROR_COUNT);
        
        // 记录处理时间
        metrics_->recordTimer(Metrics::MESSAGE_LATENCY, start);
        
        // 返回错误响应
        response->set_error_code(im::common::protocol::ErrorCode::ERROR_CODE_INTERNAL_ERROR);
        response->set_error_message(std::string("Internal error: ") + e.what());
        response->set_accepted(false);
    }
}

void MessageRouter::registerService(const ServiceInstance& instance) {
    // 注册到服务发现
    discovery_->registerServiceInstance(instance);
    metrics_->incrementCounter(Metrics::SERVICE_COUNT);
}

void MessageRouter::unregisterService(const std::string& service_id) {
    // 从服务发现中移除
    discovery_->unregisterServiceInstance(service_id);
    metrics_->decrementCounter(Metrics::SERVICE_COUNT);
}

MessageRouter::ServiceInstanceList MessageRouter::getServiceInstances(const std::string& service_name) {
    return discovery_->getServiceInstances(service_name);
}

void MessageRouter::checkStatus(StatusResponse *response) {
    response->set_is_healthy(true);
    response->set_queue_size(static_cast<int32_t>(message_queue_->size()));
    int64_t now = std::chrono::steady_clock::now().time_since_epoch().count() / 1000;
    response->set_uptime_seconds(now - start_time_);
}

std::unordered_map<std::string, int64_t> MessageRouter::getStats() {
    std::unordered_map<std::string, int64_t> stats;
    
    stats["message_count"] = message_count_;
    stats["message_error_count"] = message_error_count_;
    stats["queue_size"] = message_queue_->size();
    
    int64_t now = std::chrono::steady_clock::now().time_since_epoch().count() / 1000;
    stats["uptime_seconds"] = now - start_time_;
    
    return stats;
}

void MessageRouter::routeMessageInternal(const RouteRequest *request, RouteResponse *response) {
    if (nullptr == request || nullptr == response) {
        std::cerr << "request or response is nullptr" << std::endl;
        return;
    }

    im::common::protocol::BaseMessage base_message = request->base_message();

    // 检查目标服务
    const std::string& target_service = base_message.target_service();
    if (target_service.empty()) {
        response->set_error_code(im::common::protocol::ErrorCode::ERROR_CODE_INVALID_REQUEST);
        response->set_error_message("Target service is required");
        response->set_accepted(false);
        return;
    }

    // 获取服务实例列表
    auto instances = discovery_->getServiceInstances(target_service);
    if (instances.empty()) {
        response->set_error_code(im::common::protocol::ErrorCode::ERROR_CODE_SERVICE_UNAVAILABLE);
        response->set_error_message("No available service instances for target service");
        response->set_accepted(false);
        return;
    }

    // 选择服务实例
    auto selected_instance = load_balancer_->selectInstance(instances);
    if (!selected_instance) {
        response->set_error_code(im::common::protocol::ErrorCode::ERROR_CODE_SERVICE_UNAVAILABLE);
        response->set_error_message("Failed to select service instance");
        response->set_accepted(false);
        return;
    }

    // 路由消息到选中的服务实例
    routeMessageToInstance(request, response, selected_instance);

    // 模拟消息路由成功
    std::cout << "Routing message to service: " << target_service 
              << " instance: " << selected_instance->getServiceId() 
              << " target: " << selected_instance->getTarget() << std::endl;
    
    // 更新服务实例负载
    selected_instance->setLoad(selected_instance->getLoad() + 1);
    
    // 返回成功响应
    response->set_error_code(im::common::protocol::ErrorCode::ERROR_CODE_SUCCESS);
    response->set_error_message("Success");
    response->set_accepted(true);
}

void MessageRouter::routeMessageToInstance(const RouteRequest *request, RouteResponse *response, const ServiceInstancePtr &instance) {
    if (nullptr == request || nullptr == response || nullptr == instance) {
        std::cerr << "request, response or instance is nullptr" << std::endl;
        return;
    }

    switch (instance->getType()) {
        case ServiceType::Routing:
            break;
        case ServiceType::User:
            routeMessageToUserService(request, response, instance);
            break;
        case ServiceType::Pay:
            break;
        default:
            std::cerr << "Unknown service type: " << static_cast<int>(instance->getType()) << std::endl;
            return;
    }
}

void MessageRouter::routeMessageToUserService(const RouteRequest *request, RouteResponse *response, const ServiceInstancePtr &instance) {
    if (nullptr == request || nullptr == response || nullptr == instance) {
        std::cerr << "request, response or instance is nullptr" << std::endl;
        return;
    }

    // 调用用户服务的Stub
    auto user_stub = ServiceFactory::GetUserServiceStub(instance->getConfig());
    if (nullptr == user_stub) {
        std::cerr << "Failed to create user service stub" << std::endl;
        return;
    }

    switch (request->base_message().message_type())
    {
    case im::common::protocol::MessageType::MESSAGE_TYPE_LOGIN:
        routerMessageToUserServiceLogin(request, response, user_stub);
        break;
    default:
        break;
    }
}

void MessageRouter::routerMessageToUserServiceLogin(const RouteRequest *request, RouteResponse *response, std::unique_ptr<im::common::protocol::UserService::Stub> &user_stub)
{
    if (nullptr == request || nullptr == response || nullptr == user_stub) {
        std::cerr << "request, response or user_stub is nullptr" << std::endl;
        return;
    }

    // 调用用户服务的Login方法
    im::common::protocol::LoginRequest login_request;
    im::common::protocol::LoginResponse login_response;
    grpc::ClientContext context;
    grpc::Status status = user_stub->Login(&context, login_request, &login_response);
    if (!status.ok()) {
        std::cerr << "UserService Login failed: " << status.error_message() << std::endl;
        return;
    }

    // 处理登录响应
    if (login_response.error_code() == im::common::protocol::ErrorCode::ERROR_CODE_SUCCESS) {
        response->set_error_code(im::common::protocol::ErrorCode::ERROR_CODE_SUCCESS);
        response->set_error_message("Login successful");
        response->set_accepted(true);
    } else {
        response->set_error_code(login_response.error_code());
        response->set_error_message(login_response.error_message());
        response->set_accepted(false);
    }
}

void MessageRouter::registerDefaultServices()
{
    // 注册默认的服务实例（用于测试）
    ServiceConfig serviceUserConfig = {
        .name = "service_user",
        .id = "service_user_1",
        .target = "localhost:50053",
        .timeout = std::chrono::milliseconds(5000),
        .keepalive_time_s = 30,
        .keepalive_timeout_s = 10
    };
    std::vector<ServiceInstance> default_services = {
        ServiceInstance(serviceUserConfig, ServiceType::User),
    };
    
    for (const auto& service : default_services) {
        registerService(service);
    }
}

void MessageRouter::startHeartbeatTimer() {
    heartbeat_timer_ = std::make_shared<boost::asio::steady_timer>(*io_context_);
    // 立即执行一次心跳检测
    performHeartbeat();
    std::cout << "Heartbeat timer started" << std::endl;
}

void MessageRouter::stopHeartbeatTimer() {
    if (heartbeat_timer_) {
        heartbeat_timer_->cancel();
        std::cout << "Heartbeat timer stopped" << std::endl;
    }
}

void MessageRouter::performHeartbeat() {
    // 调用服务发现的心跳检测
    discovery_->heartbeat();
    std::cout << "Heartbeat check performed" << std::endl;
    
    // 30秒后再次执行心跳检测
    if (heartbeat_timer_) {
        heartbeat_timer_->expires_after(std::chrono::seconds(30));
        auto self = this;
        heartbeat_timer_->async_wait([self](const boost::system::error_code& ec) {
            if (!ec) {
                self->performHeartbeat();
            }
        });
    }
}

} // namespace routing
