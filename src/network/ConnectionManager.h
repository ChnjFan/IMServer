#pragma once

#include <memory>
#include <vector>
#include <functional>
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <any>
#include <queue>
#include <string>
#include <shared_mutex>
#include <stdexcept>

#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>

namespace network {

/**
 * @brief 连接状态枚举
 */
enum class ConnectionState {
    Disconnected,    // 未连接
    Connecting,      // 连接中
    Connected,       // 已连接
    Disconnecting,   // 断开中
    Error           // 错误状态
};

/**
 * @brief 将ConnectionState枚举转换为字符串
 * 
 * @param state 连接状态枚举
 * @return std::string 连接状态的字符串表示
 */
inline std::string connectionStateToString(ConnectionState state) {
    switch (state) {
        case ConnectionState::Disconnected: return "Disconnected";
        case ConnectionState::Connecting: return "Connecting";
        case ConnectionState::Connected: return "Connected";
        case ConnectionState::Disconnecting: return "Disconnecting";
        case ConnectionState::Error: return "Error";
        default: return "Unknown";
    }
};

/**
 * @brief 将ConnectionType枚举转换为字符串
 * 
 * @param type 连接类型枚举
 * @return std::string 连接类型的字符串表示
 */
inline std::string connectionTypeToString(ConnectionType type) {
    switch (type) {
        case ConnectionType::TCP: return "TCP";
        case ConnectionType::WebSocket: return "WebSocket";
        case ConnectionType::HTTP: return "HTTP";
        default: return "Unknown";
    }
}

/**
 * @brief 连接类型枚举
 */
enum class ConnectionType {
    TCP,
    WebSocket,
    HTTP
};

enum class ConnectionEvent {
    Connected,      // 已连接
    Disconnected,   // 已断开
    Removed,        // 已移除
    Error,          // 错误
};

/**
 * @brief 将ConnectionEvent枚举转换为字符串
 * 
 * @param event 连接事件枚举
 * @return std::string 连接事件的字符串表示
 */
inline std::string connectionEventToString(ConnectionEvent event) {
    switch (event) {
        case ConnectionEvent::Connected: return "Connected";
        case ConnectionEvent::Disconnected: return "Disconnected";
        case ConnectionEvent::Removed: return "Removed";
        case ConnectionEvent::Error: return "Error";
        default: return "Unknown";
    }
}

/**
 * @brief 连接统计信息
 */
struct ConnectionStats {
    uint64_t bytes_sent = 0;           // 发送字节数
    uint64_t bytes_received = 0;       // 接收字节数
    uint64_t messages_sent = 0;        // 发送消息数
    uint64_t messages_received = 0;    // 接收消息数
    std::chrono::steady_clock::time_point connected_time; // 连接时间
    std::chrono::steady_clock::time_point last_activity_time; // 最后活动时间
};

using ConnectionId = uint64_t;

/**
 * @brief 连接类基类
 * 
 * 该类封装了连接的基本功能，包括连接ID、类型、状态、统计信息等。
 * 它是所有具体连接类型（如TCP、WebSocket、Http）的基类，提供了统一的接口。
 */
class Connection : public std::enable_shared_from_this<Connection> {
public:
    using Ptr = std::shared_ptr<Connection>;
    // 消息处理回调函数
    using MessageHandler = std::function<size_t(ConnectionId, const std::vector<char>&)>;
    // 连接状态变更回调函数
    using StateChangeHandler = std::function<void(ConnectionId, ConnectionState, ConnectionState)>;
    // 连接关闭回调函数
    using CloseHandler = std::function<void(ConnectionId, const boost::system::error_code&)>;

protected:
    ConnectionId connection_id_;            // 连接ID
    ConnectionType connection_type_;        // 连接类型
    ConnectionState state_;                 // 连接状态
    ConnectionStats stats_;                 // 连接统计信息
    
    // 回调函数
    MessageHandler message_handler_;
    StateChangeHandler state_change_handler_;
    CloseHandler close_handler_;
    
    // 扩展数据存储
    std::unordered_map<std::string, std::any> contexts_;
    
    // 互斥锁（保护状态和统计数据）
    mutable std::shared_mutex state_mutex_;

public:
    Connection(ConnectionId id, ConnectionType type);
    virtual ~Connection() = default;

    // 禁止拷贝和赋值
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    // 基础信息获取
    ConnectionId getId() const { return connection_id_; }
    ConnectionType getType() const { return connection_type_; }
    ConnectionState getState() const { return state_; }
    
    // 远程端点信息
    virtual boost::asio::ip::tcp::endpoint getRemoteEndpoint() const = 0;
    virtual std::string getRemoteAddress() const = 0;
    virtual uint16_t getRemotePort() const = 0;

    // 连接生命周期管理
    virtual void start() = 0;
    virtual void close() = 0;
    virtual void forceClose() = 0;

    // 数据传输
    virtual void send(const std::vector<char>& data) = 0;
    virtual void send(const std::string& data) = 0;
    virtual void send(std::vector<char>&& data) = 0;

    // 连接状态查询
    virtual bool isConnected() const = 0;
    virtual bool isActive() const; // 检查连接是否活跃
    virtual bool isOpen() const { return isConnected(); } // 兼容性方法

    // 回调函数设置
    void setMessageHandler(MessageHandler handler) { message_handler_ = std::move(handler); }
    void setStateChangeHandler(StateChangeHandler handler) { state_change_handler_ = std::move(handler); }
    void setCloseHandler(CloseHandler handler) { close_handler_ = std::move(handler); }

    // 统计信息获取
    const ConnectionStats& getStats() const { return stats_; }
    ConnectionStats& getStats() { return stats_; }

    // 扩展数据管理
    void setContext(const std::string& key, std::any value) { contexts_[key] = std::move(value); }
    template<typename T>
    T getContext(const std::string& key) const {
        auto it = contexts_.find(key);
        if (it != contexts_.end()) {
            return std::any_cast<T>(it->second);
        }
        throw std::runtime_error("Context key not found: " + key);
    }
    bool hasContext(const std::string& key) const { return contexts_.find(key) != contexts_.end(); }
    void removeContext(const std::string& key) { contexts_.erase(key); }

protected:
    // 状态变更辅助方法
    void setState(ConnectionState new_state);
    void updateLastActivity() { stats_.last_activity_time = std::chrono::steady_clock::now(); }
    
    // 回调触发辅助方法, 返回处理的字节数
    size_t triggerMessageHandler(const std::vector<char>& data);
    void triggerCloseHandler(const boost::system::error_code& ec);
    
    // 统计信息更新
    void updateBytesSent(size_t bytes) { 
        stats_.bytes_sent += bytes; 
        updateLastActivity(); 
    }
    void updateBytesReceived(size_t bytes) { 
        stats_.bytes_received += bytes; 
        updateLastActivity(); 
    }
    void incrementMessagesSent() { 
        stats_.messages_sent++; 
        updateLastActivity(); 
    }
    void incrementMessagesReceived() { 
        stats_.messages_received++; 
        updateLastActivity(); 
    }
};

/**
 * @brief 连接管理器类
 * 
 * 该类负责管理所有连接会话，包括添加、移除、查询连接，以及处理连接状态变更和关闭事件。
 */
class ConnectionManager {
public:
    struct GlobalStats {
        size_t total_connections = 0;
        size_t active_connections = 0;
        size_t tcp_connections = 0;
        size_t websocket_connections = 0;
        size_t http_connections = 0;
        uint64_t total_bytes_sent = 0;
        uint64_t total_bytes_received = 0;
        uint64_t total_messages_sent = 0;
        uint64_t total_messages_received = 0;
        std::chrono::steady_clock::time_point start_time;
    };

private:
    // 线程安全的连接存储
    mutable std::shared_mutex connections_mutex_;
    std::unordered_map<ConnectionId, Connection::Ptr> connections_;

    // 全局统计
    mutable std::mutex stats_mutex_;
    GlobalStats global_stats_;
    
    // 配置参数
    size_t max_connections_ = 10000;
    std::chrono::seconds idle_timeout_{300}; // 5分钟
    bool enable_statistics_ = true;
    
    // 事件处理器
    std::function<void(ConnectionId, ConnectionEvent)> event_handler_;
    
    // 清理定时器
    std::unique_ptr<boost::asio::steady_timer> cleanup_timer_;

public:
    ConnectionManager();
    ~ConnectionManager();

    // 禁止拷贝和赋值
    ConnectionManager(const ConnectionManager&) = delete;
    ConnectionManager& operator=(const ConnectionManager&) = delete;

    // 连接注册和管理
    void addConnection(Connection::Ptr connection);
    void removeConnection(ConnectionId connection_id);
    void removeConnection(const Connection::Ptr& connection);
    
    // 连接查询
    Connection::Ptr getConnection(ConnectionId connection_id) const;
    std::vector<Connection::Ptr> getConnectionsByType(ConnectionType type) const;
    std::vector<Connection::Ptr> getConnectionsByState(ConnectionState state) const;
    std::vector<Connection::Ptr> getAllConnections() const;
    
    // 连接数量统计
    size_t getConnectionCount() const;
    size_t getConnectionCount(ConnectionType type) const;
    size_t getConnectionCount(ConnectionState state) const;
    
    // 连接管理操作
    void closeAllConnections();
    void closeConnectionsByType(ConnectionType type);
    void closeIdleConnections(std::chrono::seconds idle_timeout);
    
    const GlobalStats& getGlobalStats() const { return global_stats_; }
    
    // 配置管理
    void setMaxConnections(size_t max_connections);
    void setIdleTimeout(std::chrono::seconds idle_timeout);
    void setEnableStatistics(bool enable);
    
    // 事件通知
    void setConnectionEventHandler(std::function<void(ConnectionId, ConnectionEvent)> handler);
    
    // 定时器管理
    void initializeCleanupTimer(boost::asio::io_context& io_context);
    
private:
    // 内部管理方法
    void updateGlobalStats();
    void cleanupClosedConnections();
    void checkIdleConnections();
    void scheduleCleanupTask();
};

} // namespace network