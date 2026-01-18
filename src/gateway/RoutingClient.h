#pragma once

#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>

#include "gateway_routing.grpc.pb.h"

namespace gateway {

/**
 * @brief 路由服务客户端
 * 负责与消息路由层通信，发送消息路由请求
 */
class RoutingClient {
public:
    /**
     * @brief 构造函数
     * @param server_address 路由服务地址
     */
    explicit RoutingClient(const std::string& server_address);
    
    /**
     * @brief 发送路由请求
     * @param request 路由请求
     * @param response 路由响应
     * @return 是否成功
     */
    bool routeMessage(const im::common::protocol::RouteRequest& request, 
                     im::common::protocol::RouteResponse& response);
    
    /**
     * @brief 检查路由服务状态
     * @param response 状态响应
     * @return 是否成功
     */
    bool checkStatus(im::common::protocol::StatusResponse& response);
    
private:
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<im::common::protocol::RoutingService::Stub> stub_;
};

} // namespace gateway
