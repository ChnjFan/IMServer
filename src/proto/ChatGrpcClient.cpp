//
// Created by Fan on 2026/5/18.
//

#include "ChatGrpcClient.h"
#include "ConfigMgr.h"
#include "const.h"

ChatGrpcClient::ChatGrpcClient() {
    auto& config = ConfigMgr::getInstance();
    size_t size = DEFAULT_RPC_POOL_SIZE;
    if (!config["ChatServer"]["RPCConnPoolSize"].empty()) {
        size = std::stoi(config["GateServer"]["RPCConnPoolSize"]);
    }

    if (config["PeerChatServers"]["Servers"].empty()) {
        return;
    }

    auto services = config["PeerChatServers"]["Servers"];
    std::string service;
    std::stringstream ss(services);
    while (std::getline(ss, service, ',')) {
        if (config[service]["Name"].empty()) {
            continue;
        }
        auto host = config[service]["Host"];
        auto port = config[service]["Port"];
        auto pool = std::make_unique<ServiceConnPool<ChatService>>(size, host, port);
        pools_.insert({config[service]["Name"], std::move(pool)});
    }
}

AddFriendRsp ChatGrpcClient::NotifyAddFriend(std::string &serviceName, const AddFriendReq &request) {
    ClientContext context;
    AddFriendRsp reply;

    if (pools_.find(serviceName) == pools_.end()) {
        reply.set_error(static_cast<int32_t>(ErrorCodes::RPC_FAILED));
        return reply;
    }
    auto stub = pools_[serviceName]->getConnection();
    if (nullptr == stub) {
        std::cout << "Error get rpc connection" << std::endl;
        reply.set_error(static_cast<int32_t>(ErrorCodes::RPC_FAILED));
        return reply;
    }

    Defer defer([this, &stub, &serviceName] () {
        pools_[serviceName]->returnConnection(std::move(stub));
    });

    if (const auto status = stub->NotifyAddFriend(&context, request, &reply); status.ok()) {
        std::cout << "Get RPC connection success" << std::endl;
        return reply;
    }

    reply.set_error(static_cast<int32_t>(ErrorCodes::RPC_FAILED));
    return reply;
}

AuthFriendRsp ChatGrpcClient::NotifyAuthFriend(std::string &serviceName, const AuthFriendReq &request) {
    ClientContext context;
    AuthFriendRsp reply;

    if (pools_.find(serviceName) == pools_.end()) {
        reply.set_error(static_cast<int32_t>(ErrorCodes::RPC_FAILED));
        return reply;
    }
    auto stub = pools_[serviceName]->getConnection();
    if (nullptr == stub) {
        std::cout << "Error get rpc connection" << std::endl;
        reply.set_error(static_cast<int32_t>(ErrorCodes::RPC_FAILED));
        return reply;
    }

    Defer defer([this, &stub, &serviceName] () {
        pools_[serviceName]->returnConnection(std::move(stub));
    });

    if (const auto status = stub->NotifyAuthFriend(&context, request, &reply); status.ok()) {
        std::cout << "Get RPC connection success" << std::endl;
        return reply;
    }

    reply.set_error(static_cast<int32_t>(ErrorCodes::RPC_FAILED));
    return reply;
}
