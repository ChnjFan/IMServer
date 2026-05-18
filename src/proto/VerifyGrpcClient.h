//
// Created by Fan on 2026/5/4.
//

#ifndef IMSERVER_VERIFYGRPCCLIENT_H
#define IMSERVER_VERIFYGRPCCLIENT_H

#include <memory>
#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <grpcpp/grpcpp.h>

#include "message.pb.h"
#include "message.grpc.pb.h"

#include "Singleton.h"
#include "ServiceConnPool.h"

using grpc::Channel;
using grpc::Status;
using grpc::ClientContext;

using message::GetVerifyReq;
using message::GetVerifyRsp;
using message::VerifyService;

class RPCConnPool {
public:
    RPCConnPool(std::size_t size, const std::string &host, const std::string &port);
    ~RPCConnPool();

    void close();
    std::unique_ptr<VerifyService::Stub> getConnection();
    void returnConnection(std::unique_ptr<VerifyService::Stub> stub);

    RPCConnPool(const RPCConnPool&) = delete;
    RPCConnPool& operator=(const RPCConnPool&) = delete;
private:
    std::atomic<bool> stop_{false};
    std::size_t size_;
    std::string endpoint_;
    std::queue<std::unique_ptr<VerifyService::Stub>> connections_;
    // 控制队列线程安全
    std::condition_variable cv_;
    std::mutex mutex_;
};

class VerifyGrpcClient : public Singleton<VerifyGrpcClient> {
public:
    [[nodiscard]] GetVerifyRsp GetVerifyCode(std::string email) const;

private:
    friend class Singleton<VerifyGrpcClient>;

    VerifyGrpcClient();

    std::unique_ptr<ServiceConnPool<VerifyService>> connPool_;
};


#endif //IMSERVER_VERIFYGRPCCLIENT_H