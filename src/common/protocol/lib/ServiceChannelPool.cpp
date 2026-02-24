#include "ServiceChannelPool.h"

#include <grpcpp/channel.h>
#include <grpcpp/create_channel.h>

ServiceChannelPool &ServiceChannelPool::Instance() {
    static ServiceChannelPool instance;
    return instance;
}

grpc::ChannelArguments ServiceChannelPool::BuildChannelArgs(const ServiceConfig& cfg) {
    grpc::ChannelArguments args;
    // 设置长连接保活参数（gRPC基于HTTP/2，保活避免TCP连接被防火墙断开）
    args.SetInt("grpc.keepalive_time_ms", cfg.keepalive_time_s * 1000);
    args.SetInt("grpc.keepalive_timeout_ms", cfg.keepalive_timeout_s * 1000);
    args.SetInt("grpc.keepalive_permit_without_calls", 1); // 无调用时也发送保活包
    return args;
}

std::shared_ptr<grpc::Channel> ServiceChannelPool::GetChannel(const ServiceConfig& cfg) {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = channel_map_.find(cfg.target);
        if (it != channel_map_.end()) {
            return it->second;
        }
    }

    // 重新检查，保证线程安全
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = channel_map_.find(cfg.target);
    if (it != channel_map_.end()) {
        return it->second;
    }

    // 创建新Channel：基于Target和配置参数
    // 生产环境替换为TLS认证：grpc::SslCredentials(grpc::SslCredentialsOptions{})
    auto channel = grpc::CreateCustomChannel(
        cfg.target,
        grpc::InsecureChannelCredentials(),
        BuildChannelArgs(cfg)
    );

    // 验证Channel是否就绪（可选，阻塞直到就绪或超时）
    if (!channel->WaitForConnected(
        std::chrono::system_clock::now() + std::chrono::seconds(5)
    )) {
        throw std::runtime_error("create grpc channel failed, target: " + cfg.target 
                                 + ", error: timeout");
    }

    // 存入Channel池并返回
    channel_map_[cfg.target] = channel;
    return channel;
}

// 关闭所有Channel：释放资源，程序退出时调用
void ServiceChannelPool::CloseAll() {
    std::lock_guard<std::mutex> lock(mtx_);
    channel_map_.clear(); // shared_ptr析构时自动关闭Channel
}