#pragma once

#ifndef SERVICE_FACTORY_H
#define SERVICE_FACTORY_H

#include <grpcpp/grpcpp.h>

#include "ServiceInstance.h"
#include "ServiceChannelPool.h"

#include "gateway_routing.grpc.pb.h"

class ServiceFactory {
public:
    // 获取RoutingService的Stub（基于routing服务的共享Channel）
    static std::unique_ptr<im::common::protocol::RoutingService::Stub> GetRoutingServiceStub(ServiceConfig &config) {
        auto channel = GrpcChannelPool::Instance().GetChannel(config);
        return im::common::protocol::RoutingService::NewStub(channel);
    }

    static std::unique_ptr<im::common::protocol::UserService::Stub> GetUserServiceStub(ServiceConfig &config) {
        auto channel = GrpcChannelPool::Instance().GetChannel(config);
        return im::common::protocol::UserService::NewStub(channel);
    }

    // 新增服务时，仅需添加此类型的工厂方法，无需修改其他代码！
    // 示例：新增支付服务
    // static std::unique_ptr<pay::PayService::Stub> GetPayServiceStub(ServiceConfig &config) {
    //     auto channel = GrpcChannelPool::Instance().GetChannel(config);
    //     return pay::PayService::NewStub(channel);
    // }
};

#endif // SERVICE_FACTORY_H