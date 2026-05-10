//
// Created by Fan on 2026/5/10.
//

#ifndef IMSERVER_STATUSGRPCCLIENT_H
#define IMSERVER_STATUSGRPCCLIENT_H

#include <memory>
#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <grpcpp/grpcpp.h>

#include "message.pb.h"
#include "message.grpc.pb.h"

#include "Singleton.h"

using grpc::Channel;
using grpc::Status;
using grpc::ClientContext;

using message::GetChatServerReq;
using message::GetChatServerRsp;
using message::StatusService;

class StatusConnPool {
public:
    StatusConnPool(std::size_t size, const std::string &host, const std::string &port);
    ~StatusConnPool();

    void close();
    std::unique_ptr<StatusService::Stub> getConnection();
    void returnConnection(std::unique_ptr<StatusService::Stub> stub);

    StatusConnPool(const StatusConnPool&) = delete;
    StatusConnPool& operator=(const StatusConnPool&) = delete;
private:
    std::atomic<bool> stop_{false};
    std::size_t size_;
    std::string endpoint_;
    std::queue<std::unique_ptr<StatusService::Stub>> connections_;
    // 控制队列线程安全
    std::condition_variable cv_;
    std::mutex mutex_;
};

class StatusGrpcClient : public Singleton<StatusGrpcClient> {
public:
    GetChatServerRsp GetChatServer(int uid);

private:
    friend class Singleton<StatusGrpcClient>;

    StatusGrpcClient();

    std::unique_ptr<StatusConnPool> pool_;
};


#endif //IMSERVER_STATUSGRPCCLIENT_H