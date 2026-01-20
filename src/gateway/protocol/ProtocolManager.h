#pragma once

#include <memory>
#include <vector>
#include <functional>
#include <unordered_map>
#include <mutex>

#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>

#include "ConnectionManager.h"
#include "Parser.h"
#include "Message.h"
#include "MessageRouter.h"
#include "AsyncExecutor.h"

namespace protocol {

class ProtocolManager {
private:
    std::unordered_map<network::ConnectionId, std::shared_ptr<Parser>> parsers_; // 解析器映射（按连接ID）
    MessageRouter message_router_; // 消息路由器
    AsyncExecutor executor_; // 异步执行器
    network::ConnectionManager& connection_manager_;
    std::mutex mutex_; // 互斥锁，保护成员变量

public:
    ProtocolManager(network::ConnectionManager& connection_manager);
    ~ProtocolManager() = default;

    ProtocolManager(const ProtocolManager&) = delete;
    ProtocolManager& operator=(const ProtocolManager&) = delete;

    /**
     * @brief 异步处理数据
     * @tparam CompletionToken 完成回调类型
     * @param connection_id 连接ID
     * @param data 待处理的数据
     * @param token 完成回调
     * @return 异步操作的返回值
     */
    template<typename CompletionToken>
    auto asyncProcessData(network::ConnectionId connection_id, std::vector<char> data, CompletionToken&& token) {
        // 使用异步操作包装器
        boost::asio::async_completion<CompletionToken, 
            void(const boost::system::error_code&)> init(token);
        
        // 异步提交处理任务到IO线程池，因为解析任务主要是IO密集型
        executor_.submitIO([this, connection_id, data = std::move(data), 
            handler = std::move(init.completion_handler)]() mutable {
            doProcessData(connection_id, std::move(data), std::move(handler));
        });
        
        return init.result.get();
    }

    /**
     * @brief 注册消息处理器
     * @param connection_type 消息类型
     * @param handler 消息处理器回调函数
     */
    void registerHandler(network::ConnectionType connection_type, std::function<void(const Message&, network::Connection::Ptr)> handler);

    /**
     * @brief 获取协议解析器
     * @param connection_id 连接ID
     * @param connection_type 连接类型
     * @return std::shared_ptr<Parser> 协议解析器指针
     */
    std::shared_ptr<Parser> getParser(network::ConnectionId connection_id, network::ConnectionType connection_type);

    /**
     * @brief 设置异步执行器的线程池大小
     * @param io_threads IO线程池大小
     * @param cpu_threads CPU线程池大小
     */
    void setThreadPoolSize(size_t io_threads, size_t cpu_threads);

private:
    /**
     * @brief 内部异步处理函数
     * @param connection_id 连接ID
     * @param data 待处理的数据
     * @param callback 处理完成回调
     */
    void doProcessData(network::ConnectionId connection_id, std::vector<char> data,
                        std::function<void(const boost::system::error_code&)> callback);

    void initializeParsers();
};

} // namespace protocol
