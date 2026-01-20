#pragma once

#include <memory>
#include <string>

#include "gateway_routing.grpc.pb.h"
#include "MessageRouter.h"

namespace routing {

using grpc::ServerUnaryReactor;
using grpc::ServerBidiReactor;
using im::common::protocol::RouteRequest;
using im::common::protocol::RouteResponse;
using im::common::protocol::StatusResponse;

class RoutingServiceImpl final : public im::common::protocol::RoutingService::CallbackService {
    ServerUnaryReactor* RouteMessage(grpc::CallbackServerContext* context, 
                                     const RouteRequest* request, 
                                     RouteResponse* response) override;
    ServerBidiReactor<RouteRequest, RouteResponse>* BatchRouteMessages(grpc::CallbackServerContext* context) override;
    ServerUnaryReactor* CheckStatus(grpc::CallbackServerContext* context, 
                                     const google::protobuf::Empty* request, 
                                     StatusResponse* response) override;
}

/**
 * @brief 路由服务类
 * 实现gRPC服务端接口，处理来自网关的路由请求
 */
class RoutingService final : public im::common::protocol::RoutingService::Service {
public:
    explicit RoutingService(int port = 50050);
    ~RoutingService() override;

    bool start();
    void stop();
    bool isRunning() const { return running_; }

    int getPort() const { return port_; }

    /**
     * @brief 获取消息路由器
     * @return 消息路由器
     */
    MessageRouter* getMessageRouter() { return message_router_.get(); }

private:
    std::unique_ptr<grpc::Server> server_;
    // 服务器线程
    std::thread server_thread_;
    // 消息路由器
    std::unique_ptr<MessageRouter> message_router_;
    // 路由服务实现
    RoutingServiceImpl service_;
    // 服务端口
    int port_;
    // 服务是否运行
    bool running_;

    void runServer();
};

} // namespace routing
