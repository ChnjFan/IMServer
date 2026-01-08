#pragma once

#include <memory>
#include <vector>
#include <functional>
#include <unordered_map>
#include <shared_mutex>

#include "Message.h"
#include "AsyncExecutor.h"

namespace protocol {

/**
 * @brief 消息路由器类
 * 负责根据消息类型将消息路由到对应的处理器
 */
class MessageRouter {
public:
    /**
     * @brief 消息处理器类型
     */
    using MessageHandler = std::function<void(const Message&, network::Connection::Ptr)>;

private:
    // 路由表，消息类型到处理器的映射
    std::unordered_map<Message::network::ConnectionType, MessageHandler> handlers_;
    // 读写锁，支持并发访问路由表
    std::shared_mutex mutex_;
    // 异步执行器
    std::shared_ptr<AsyncExecutor> executor_;
    // 默认执行器（如果未设置外部执行器）
    std::shared_ptr<AsyncExecutor> default_executor_;

public:
    MessageRouter();
    /**
     * @brief 注册消息处理器
     * @param message_type 消息类型
     * @param handler 消息处理器回调函数
     */
    void registerHandler(Message::network::ConnectionType message_type, MessageHandler handler);
    
    /**
     * @brief 移除消息处理器
     * @param message_type 消息类型
     */
    void removeHandler(Message::network::ConnectionType message_type);
    
    /**
     * @brief 异步路由消息
     * @param message 消息对象
     * @param connection 关联的连接对象
     */
    void asyncRoute(const Message& message, network::Connection::Ptr connection);
    
    /**
     * @brief 同步路由消息（阻塞调用）
     * @param message 消息对象
     * @param connection 关联的连接对象
     */
    void route(const Message& message, network::Connection::Ptr connection);
    
    /**
     * @brief 检查是否存在指定类型的处理器
     * @param message_type 消息类型
     * @return bool 是否存在处理器
     */
    bool hasHandler(Message::network::ConnectionType message_type);

    /**
     * @brief 设置异步执行器
     * @param executor 异步执行器
     */
    void setExecutor(std::shared_ptr<AsyncExecutor> executor) {
        executor_ = std::move(executor);
    }
};

} // namespace protocol
