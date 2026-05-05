//
// Created by Fan on 2026/5/4.
//

#include "VerifyGrpcClient.h"
#include "const.h"
#include "ConfigMgr.h"

RPCConnPool::RPCConnPool(const std::size_t size, const std::string &host, const std::string &port)
    : size_(size), endpoint_(host + ":" + port) {
    std::cout << "Creating RPC Connector, size = " << size << std::endl;
    for (std::size_t i = 0; i < size; ++i) {
        const auto channel = grpc::CreateChannel(endpoint_,
                                                                 grpc::InsecureChannelCredentials());
        connections_.push(VerifyService::NewStub(channel));
    }
}

RPCConnPool::~RPCConnPool() {
    std::lock_guard<std::mutex> lock(mutex_);
    close();
    while (!connections_.empty()) {
        connections_.pop();
    }
}

void RPCConnPool::close() {
    stop_.store(true);
    cv_.notify_all();
}

std::unique_ptr<VerifyService::Stub> RPCConnPool::getConnection() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] {
        if (stop_.load()) {// 已经停止继续执行
            return true;
        }
        return !connections_.empty();
    });
     if (stop_.load()) {
         return nullptr;
     }
    auto connection = std::move(connections_.front());
    connections_.pop();
    std::cout << "Get RPC connection success" << std::endl;
    return connection;
}

// 使用完成后返还连接
void RPCConnPool::returnConnection(std::unique_ptr<VerifyService::Stub> stub) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stop_.load()) {
        return;
    }
    connections_.push(std::move(stub));
    std::cout << "Returning RPC connection success" << std::endl;
}

GetVerifyRsp VerifyGrpcClient::GetVerifyCode(std::string email) const {
    ClientContext context;
    GetVerifyRsp reply;
    GetVerifyReq request;

    request.set_email(email);
    auto stub = connPool_->getConnection();
    if (const Status status = stub->GetVerifyCode(&context, request, &reply); !status.ok()) {
        reply.set_error(static_cast<int32_t>(ErrorCodes::RPC_FAILED));
    }
    connPool_->returnConnection(std::move(stub));
    return reply;
}

VerifyGrpcClient::VerifyGrpcClient() {
    auto& config = ConfigMgr::getInstance();
    size_t size = std::stoi(config["GateServer"]["RPCConnPoolSize"]);
    if (0 == size) {
        size = DEFAULT_RPC_POOL_SIZE;
    }

    connPool_ = std::make_unique<RPCConnPool>(size,
        config["VerifyServer"]["Host"],
        config["VerifyServer"]["Port"]);
}
