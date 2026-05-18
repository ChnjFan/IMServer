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
