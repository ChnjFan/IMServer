#include "MessageRouter.h"

#include <stdexcept>
#include <iostream>

namespace protocol {

MessageRouter::MessageRouter() {
    // 创建默认的异步执行器
    default_executor_ = std::make_shared<AsyncExecutor>();
    executor_ = default_executor_;
}

void MessageRouter::registerHandler(
    uint16_t message_type,
    MessageHandler handler) {
    if (!handler) {
        throw std::invalid_argument("Handler cannot be null");
    }
    
    std::unique_lock<std::shared_mutex> lock(mutex_);
    handlers_[message_type] = std::move(handler);
}

void MessageRouter::removeHandler(uint16_t message_type) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    handlers_.erase(message_type);
}

void MessageRouter::asyncRoute(
    const Message& message,
    network::Connection::Ptr connection) {
    // 异步提交路由任务
    executor_->submit([this, message, connection]() {
        route(message, connection);
    });
}

void MessageRouter::route(
    const Message& message,
    network::Connection::Ptr connection) {
    MessageHandler handler;
    
    // 查找处理器
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = handlers_.find(message.getMessageType());
        if (it == handlers_.end()) {
            // 没有找到处理器，这里可以添加默认处理逻辑
            std::cerr << "No handler found for message type: " << message.getMessageType() << std::endl;
            return;
        }
        
        handler = it->second;
    }
    
    // 调用处理器
    try {
        handler(message, connection);
    } catch (const std::exception& e) {
        std::cerr << "Exception in message handler: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception in message handler" << std::endl;
    }
}

bool MessageRouter::hasHandler(uint16_t message_type) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return handlers_.find(message_type) != handlers_.end();
}

} // namespace protocol
