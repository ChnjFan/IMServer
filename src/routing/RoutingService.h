#pragma once

#include <memory>
#include <string>
#include <thread>

#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include "gateway_routing.grpc.pb.h"
#include "MessageRouter.h"

namespace routing {

using grpc::ServerUnaryReactor;
using grpc::ServerBidiReactor;
using im::common::protocol::RouteRequest;
using im::common::protocol::RouteResponse;
using im::common::protocol::StatusResponse;

class RoutingService final : public im::common::protocol::RoutingService::CallbackService {
private:
    std::unique_ptr<MessageRouter> message_router_;
    int port_;
    bool running_;
    std::unique_ptr<grpc::Server> server_;
    std::thread server_thread_;

public:
    explicit RoutingService(int port = 50050);
    ~RoutingService() override;

    bool start();
    void stop();
    bool isRunning() const { return running_; }

    int getPort() const { return port_; }
    MessageRouter* getMessageRouter() { return message_router_.get(); }

    // gRPC服务接口实现
    grpc::ServerUnaryReactor* RouteMessage(grpc::CallbackServerContext* context, 
                                         const RouteRequest* request, 
                                         RouteResponse* response) override;
    
    grpc::ServerBidiReactor<RouteRequest, RouteResponse>* BatchRouteMessages(
                                         grpc::CallbackServerContext* context) override;
    
    grpc::ServerUnaryReactor* CheckStatus(grpc::CallbackServerContext* context, 
                                         const google::protobuf::Empty* request, 
                                         StatusResponse* response) override;

private:
    void runServer();
};

} // namespace routing
