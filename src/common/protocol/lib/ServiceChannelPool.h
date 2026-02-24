#pragma once

#ifndef SERVICE_CHANNEL_POOL_H
#define SERVICE_CHANNEL_POOL_H

#include <grpcpp/grpcpp.h>
#include <unordered_map>
#include <mutex>
#include <string>

#include "ServiceInstance.h"

// 单例Channel池：管理所有gRPC Channel，按Target唯一标识
class ServiceChannelPool {
public:
    static ServiceChannelPool& Instance();

    // 获取指定Target的Channel（不存在则创建，线程安全）
    std::shared_ptr<grpc::Channel> GetChannel(const ServiceConfig& cfg);

    // 关闭所有Channel（程序退出时调用，释放资源）
    void CloseAll();

    // 禁止拷贝和赋值（单例核心）
    ServiceChannelPool(const ServiceChannelPool&) = delete;
    ServiceChannelPool& operator=(const ServiceChannelPool&) = delete;

private:
    ServiceChannelPool() = default;
    ~ServiceChannelPool() = default;

    // Channel映射：key=Target，value=共享指针管理的Channel
    std::unordered_map<std::string, std::shared_ptr<grpc::Channel>> channel_map_;
    // 读写互斥锁：保证并发访问安全（读多写少，可后续优化为读写锁）
    std::mutex mtx_;

    // 构建Channel的配置选项（基于服务配置）
    grpc::ChannelArguments BuildChannelArgs(const ServiceConfig& cfg);
};

#endif // SERVICE_CHANNEL_POOL_H
