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

ChatServiceRsp ChatGrpcClient::NotifyAddFriend(const std::string &serviceName, const ChatServiceReq &request) {
    ClientContext context;
    ChatServiceRsp reply;

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
        std::cout << "RPC NotifyAddFriend success" << std::endl;
        return reply;
    }

    reply.set_error(static_cast<int32_t>(ErrorCodes::RPC_FAILED));
    return reply;
}

ChatServiceRsp ChatGrpcClient::NotifyAuthFriend(const std::string &serviceName, const ChatServiceReq &request) {
    ClientContext context;
    ChatServiceRsp reply;

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
        std::cout << "RPC NotifyAuthFriend success" << std::endl;
        return reply;
    }

    reply.set_error(static_cast<int32_t>(ErrorCodes::RPC_FAILED));
    return reply;
}

ChatServiceRsp ChatGrpcClient::SendChatMsg(const std::string &serviceName, const ChatServiceReq &request) {
    ClientContext context;
    ChatServiceRsp reply;

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

    if (const auto status = stub->SendChatMsg(&context, request, &reply); status.ok()) {
        std::cout << "RPC SendChatMsg success" << std::endl;
        return reply;
    }

    reply.set_error(static_cast<int32_t>(ErrorCodes::RPC_FAILED));
    return reply;
}

ErrorCodes ChatGrpcClient::NotifyOffline(const std::string &serviceName, const int uid) {
    ClientContext context;
    ChatServiceReq request;
    ChatServiceRsp response;

    if (pools_.find(serviceName) == pools_.end()) {
        return ErrorCodes::RPC_FAILED;
    }
    auto stub = pools_[serviceName]->getConnection();
    if (nullptr == stub) {
        std::cout << "Error get rpc connection" << std::endl;
        return ErrorCodes::RPC_FAILED;
    }

    Defer defer([this, &stub, &serviceName] () {
        pools_[serviceName]->returnConnection(std::move(stub));
    });

    request.set_from_uid(-1);
    request.set_to_uid(uid);
    stub->NotifyOffline(&context, request, &response);
    return static_cast<ErrorCodes>(response.error());
}

