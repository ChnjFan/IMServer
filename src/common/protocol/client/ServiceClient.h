#pragma once

#ifndef SERVICE_CLIENT_H
#define SERVICE_CLIENT_H

#include "ServiceInstance.h"
#include "ServiceFactory.h"
#include "ServiceChannelPool.h"

class ServiceClient {
public:
    ServiceClient(const ServiceInstance& instance) : instance_(instance)  {}

protected:
    ServiceInstance instance_;
    std::unique_ptr<grpc::Channel> channel_;
};

class UserServiceClient : public ServiceClient {
public:
    UserServiceClient(const ServiceInstance& instance) : ServiceClient(instance) {}

private:
};

#endif // SERVICE_CLIENT_H