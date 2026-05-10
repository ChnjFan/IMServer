//
// Created by Fan on 2026/5/10.
//

#include "StatusGrpcClient.h"

#include "const.h"
#include "ConfigMgr.h"

StatusConnPool::StatusConnPool(const std::size_t size, const std::string &host, const std::string &port)
    : size_(size), endpoint_(host + ":" + port) {
    std::cout << "Creating Status connection ...";
    for (std::size_t i = 0; i < size; i++) {
        const auto channel = grpc::CreateChannel(endpoint_,
            grpc::InsecureChannelCredentials());
        connections_.push(StatusService::NewStub(channel));
    }
    std::cout << "OK" << std::endl;
}

StatusConnPool::~StatusConnPool() {
    std::lock_guard<std::mutex> lock(mutex_);
    close();
    while (!connections_.empty()) {
        connections_.pop();
    }
}

void StatusConnPool::close() {
    stop_.store(true);
    cv_.notify_all();
}

std::unique_ptr<StatusService::Stub> StatusConnPool::getConnection() {
    std::unique_lock<std::mutex> lock(mutex_);
    // todo:不能一直阻塞，需要有超时策略
    cv_.wait(lock, [this]() {
        if (stop_.load()) {
            return true;
        }
        return !connections_.empty();
    });
    if (stop_.load()) {
        return nullptr;
    }
    auto conn = std::move(connections_.front());
    connections_.pop();
    std::cout << "Get RPC connection success" << std::endl;
    return conn;
}

void StatusConnPool::returnConnection(std::unique_ptr<StatusService::Stub> stub) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stop_.load()) {
        return;
    }
    connections_.push(std::move(stub));
    std::cout << "Returning RPC connection success" << std::endl;
}

GetChatServerRsp StatusGrpcClient::GetChatServer(int uid) {
    ClientContext context;
    GetChatServerReq request;
    GetChatServerRsp reply;
    auto stub = pool_->getConnection();
    if (nullptr == stub) {
        std::cout << "Error get rpc connection" << std::endl;
        reply.set_error(static_cast<int32_t>(ErrorCodes::RPC_FAILED));
        return reply;
    }

    Defer defer([this, &stub] () {
        pool_->returnConnection(std::move(stub));
    });
    request.set_uid(uid);
    if (const auto status = stub->GetChatServer(&context, request, &reply); status.ok()) {
        std::cout << "Get RPC connection success" << std::endl;
        return reply;
    }

    reply.set_error(static_cast<int32_t>(ErrorCodes::RPC_FAILED));
    return reply;
}

StatusGrpcClient::StatusGrpcClient() {
    auto& config = ConfigMgr::getInstance();
    size_t size = DEFAULT_RPC_POOL_SIZE;
    if (!config["GateServer"]["RPCConnPoolSize"].empty()) {
        size = std::stoi(config["GateServer"]["RPCConnPoolSize"]);
    }

    std::string host = config["StatusServer"]["Host"];
    std::string port = config["StatusServer"]["Port"];
    pool_ = std::make_unique<StatusConnPool>(size, host, port);
}
