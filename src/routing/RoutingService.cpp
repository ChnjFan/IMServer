#include "RoutingService.h"
#include <iostream>
#include <sstream>
#include <thread>

namespace routing {

ServerUnaryReactor *RoutingServiceImpl::RouteMessage(grpc::CallbackServerContext *context,
        const RouteRequest *request,
        RouteResponse *response) {
    ServerUnaryReactor* reactor = context->DefaultReactor();
    if (nullptr == reactor) {
        static_assert(false, "DefaultReactor is nullptr");
        return nullptr;
    }

    try {
        if (!request) {
            reactor->Finish(grpc::Status(grpc::INVALID_ARGUMENT, "Invalid request"));
            return reactor;
        }
        
        message_router_->routeMessage(request, response);
        reactor->Finish(grpc::Status::OK);
        return reactor;
    } catch (const std::exception& e) {
        std::cerr << "Error in RouteMessage: " << e.what() << std::endl;
        reactor->Finish(grpc::Status(grpc::INTERNAL, "Internal error: " + std::string(e.what())));
        return reactor;
    }
    return nullptr;
}

ServerBidiReactor<RouteRequest, RouteResponse> *RoutingServiceImpl::BatchRouteMessages(
        grpc::CallbackServerContext *context) {
    class Reactor : public ServerBidiReactor<RouteRequest, RouteResponse> {
     public:
      explicit Reactor() { StartRead(&request_); }

      void OnReadDone(bool ok) override {
        if (!ok) {
          std::cout << "OnReadDone Cancelled!" << std::endl;
          return Finish(grpc::Status::CANCELLED);
        }

        message_router_->routeMessage(&request_, &response_);
        StartWrite(&response_);
      }

      void OnWriteDone(bool ok) override {
        if (!ok) {
          // Client cancelled it
          std::cout << "OnWriteDone Cancelled!" << std::endl;
          return Finish(grpc::Status::CANCELLED);
        }
        StartRead(&request_);
      }

      void OnDone() override { delete this; }

     private:
      RouteRequest request_;
      RouteResponse response_;
    };
    return new Reactor();
}

ServerUnaryReactor *RoutingServiceImpl::CheckStatus(grpc::CallbackServerContext *context,
         const google::protobuf::Empty *request,
         StatusResponse *response) {
    message_router_->checkStatus(response);

    ServerUnaryReactor* reactor = context->DefaultReactor();
    if (nullptr == reactor) {
        static_assert(false, "DefaultReactor is nullptr");
        return nullptr;
    }

    reactor->Finish(grpc::Status::OK);
    return reactor;
}

RoutingService::RoutingService(int port) 
    : message_router_(std::make_unique<MessageRouter>()),
      port_(port),
      running_(false) {
}

RoutingService::~RoutingService() {
    stop();
}

bool RoutingService::start() {
    if (running_) {
        return true;
    }

    try {
        std::string server_address = "0.0.0.0:" + std::to_string(port_);

        grpc::ServerBuilder builder;
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(&service_);
        // 配置同步服务器选项，设置最大poller数量为10
        builder.SetSyncServerOption(grpc::ServerBuilder::MAX_POLLERS, 10);
        
        server_ = builder.BuildAndStart();
        if (!server_) {
            std::cerr << "[routing]Failed to start RoutingService" << std::endl;
            return false;
        }
        
        running_ = true;
        server_thread_ = std::thread([this]() {
            runServer();
        });

        std::cout << "[routing]RoutingService started successfully on port " << port_ << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[routing]Error starting RoutingService: " << e.what() << std::endl;
        return false;
    }
}

void RoutingService::stop() {
    if (!running_) {
        return;
    }

    running_ = false;
    
    // 关闭服务器
    if (server_) {
        server_->Shutdown();
    }
    
    // 等待服务器线程结束
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    
    std::cout << "RoutingService stopped" << std::endl;
}

void RoutingService::runServer() {
    if (!server_) {
        return;
    }
        
    std::cout << "[routing]RoutingService is running and listening for requests" << std::endl;
    
    // 阻塞直到服务器关闭
    server_->Wait();
}

} // namespace routing
