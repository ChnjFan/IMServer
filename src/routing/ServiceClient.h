#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <string>

#include "gateway_routing.grpc.pb.h"

namespace routing {

class ServiceClient {
public:
    ServiceClient(std::string id, std::string name, std::string h, int p)
        : instance_(std::move(id), std::move(name), std::move(h), p) {
            channel_ = grpc::CreateChannel(host + ":" + std::to_string(port), grpc::InsecureChannelCredentials());
            std::cout << "[Routing] Create grpc channel to service: " << name << " at " << host << ":" << port << std::endl;
        }
private:
    ServiceInstance instance_;
    std::shared_ptr<grpc::Channel> channel_;
};

} // namespace routing