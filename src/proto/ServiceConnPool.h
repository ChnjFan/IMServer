//
// Created by Fan on 2026/5/18.
//

#ifndef IMSERVER_SERVICECONNPOOL_H
#define IMSERVER_SERVICECONNPOOL_H

#include <memory>
#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <grpcpp/grpcpp.h>

using grpc::Channel;
using grpc::Status;
using grpc::ClientContext;

template<typename ServiceType>
class ServiceConnPool {
public:
    ServiceConnPool(std::size_t size, const std::string &host, const std::string &port);
    ~ServiceConnPool();

    void close();
    std::unique_ptr<typename ServiceType::Stub> getConnection();
    void returnConnection(std::unique_ptr<typename ServiceType::Stub> stub);

    ServiceConnPool(const ServiceConnPool&) = delete;
    ServiceConnPool& operator=(const ServiceConnPool&) = delete;
private:
    std::atomic<bool> stop_{false};
    std::size_t size_;
    std::string endpoint_;
    std::queue<std::unique_ptr<typename ServiceType::Stub>> connections_{};
    // 控制队列线程安全
    std::condition_variable cv_;
    std::mutex mutex_;
};

template<typename ServiceType>
ServiceConnPool<ServiceType>::ServiceConnPool(const std::size_t size, const std::string &host, const std::string &port)
    : size_(size), endpoint_(host + ":" + port) {
    std::cout << "Creating service connection ...";
    for (std::size_t i = 0; i < size; i++) {
        const auto channel = grpc::CreateChannel(endpoint_,
            grpc::InsecureChannelCredentials());
        connections_.push(ServiceType::NewStub(channel));
    }
    std::cout << "OK" << std::endl;
}

template<typename ServiceType>
ServiceConnPool<ServiceType>::~ServiceConnPool() {
    std::lock_guard<std::mutex> lock(mutex_);
    close();
    while (!connections_.empty()) {
        connections_.pop();
    }
}

template<typename ServiceType>
void ServiceConnPool<ServiceType>::close() {
    stop_.store(true);
    cv_.notify_all();
}

template<typename ServiceType>
std::unique_ptr<typename ServiceType::Stub> ServiceConnPool<ServiceType>::getConnection() {
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
    return conn;
}

template<typename ServiceType>
void ServiceConnPool<ServiceType>::returnConnection(std::unique_ptr<typename ServiceType::Stub> stub) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stop_.load()) {
        return;
    }
    connections_.push(std::move(stub));
}

#endif //IMSERVER_SERVICECONNPOOL_H
