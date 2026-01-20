#include "RoutingClient.h"

#include <iostream>

namespace gateway {

RoutingClient::RoutingClient(const std::string& server_address) {
    channel_ = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
    stub_ = im::common::protocol::RoutingService::NewStub(channel_);
    
    std::cout << "[RoutingClient] Connected to routing service: " << server_address << std::endl;
}

bool RoutingClient::routeMessage(const im::common::protocol::RouteRequest& request, 
                                im::common::protocol::RouteResponse& response) {
    try {
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));

        grpc::Status status = stub_->RouteMessage(&context, request, &response);
        
        if (status.ok()) {
            return true;
        } else {
            std::cerr << "[RoutingClient] RouteMessage failed: " 
                      << status.error_code() << " - " << status.error_message() << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "[RoutingClient] Exception in RouteMessage: " << e.what() << std::endl;
        return false;
    }
}

bool RoutingClient::checkStatus(im::common::protocol::StatusResponse& response) {
    try {
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(2));
        
        google::protobuf::Empty request;
        grpc::Status status = stub_->CheckStatus(&context, request, &response);
        
        if (status.ok()) {
            return true;
        } else {
            std::cerr << "[RoutingClient] CheckStatus failed: " 
                      << status.error_code() << " - " << status.error_message() << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "[RoutingClient] Exception in CheckStatus: " << e.what() << std::endl;
        return false;
    }
}

} // namespace gateway
