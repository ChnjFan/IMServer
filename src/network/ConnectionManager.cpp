#include "ConnectionManager.h"
#include <algorithm>
#include <iostream>

namespace network {

// ==================== Connection类实现 ====================

Connection::Connection(ConnectionId id, ConnectionType type)
    : connection_id_(id)
    , connection_type_(type)
    , state_(ConnectionState::Disconnected) {
    stats_.connected_time = std::chrono::steady_clock::now();
    stats_.last_activity_time = stats_.connected_time;
}

bool Connection::isActive() const {
    std::shared_lock lock(state_mutex_);
    auto now = std::chrono::steady_clock::now();
    auto idle_duration = std::chrono::duration_cast<std::chrono::seconds>(
        now - stats_.last_activity_time);
    
    // 如果连接已断开或空闲时间超过30分钟，认为不活跃
    return state_ == ConnectionState::Connected && idle_duration.count() < 1800;
}

void Connection::setState(ConnectionState new_state) {
    if (state_ == new_state) {
        return;
    }
    
    auto old_state = state_;
    state_ = new_state;
    
    // 触发状态机
    if (state_change_handler_) {
        try {
            state_change_handler_(connection_id_, old_state, new_state);
        } catch (const std::exception& e) {
            std::cerr << "State change handler exception: " << e.what() << std::endl;
        }
    }
}

size_t Connection::triggerMessageHandler(const std::vector<char>& data) {
    if (message_handler_) {
        try {
            return message_handler_(connection_id_, data);
        } catch (const std::exception& e) {
            std::cerr << "Message handler exception: " << e.what() << std::endl;
        }
    }
    return 0;
}

void Connection::triggerCloseHandler(const boost::system::error_code& ec) {
    if (close_handler_) {
        try {
            close_handler_(connection_id_, ec);
        } catch (const std::exception& e) {
            std::cerr << "Close handler exception: " << e.what() << std::endl;
        }
    }
}

// ==================== ConnectionManager类实现 ====================

ConnectionManager::ConnectionManager() 
    : global_stats_{} {
    global_stats_.start_time = std::chrono::steady_clock::now();
}

ConnectionManager::~ConnectionManager() {
    // 停止清理定时器
    if (cleanup_timer_) {
        cleanup_timer_->cancel();
    }
    // 关闭所有连接
    closeAllConnections();
}

void ConnectionManager::initializeCleanupTimer(boost::asio::io_context& io_context) {
    // 初始化清理定时器
    cleanup_timer_ = std::make_unique<boost::asio::steady_timer>(io_context);
    
    // 启动定时清理任务
    scheduleCleanupTask();
}

void ConnectionManager::scheduleCleanupTask() {
    if (!cleanup_timer_) {
        return;
    }
    
    // 每30秒执行一次清理任务
    cleanup_timer_->expires_after(std::chrono::seconds(30));
    cleanup_timer_->async_wait([this](const boost::system::error_code& ec) {
        if (!ec) {
            cleanupClosedConnections();
            checkIdleConnections();
            updateGlobalStats();
            
            scheduleCleanupTask();
        }
    });
}

void ConnectionManager::addConnection(Connection::Ptr connection) {
    if (!connection) {
        throw std::invalid_argument("Connection cannot be null");
    }

    std::unique_lock lock(connections_mutex_);
    ConnectionId id = connection->getId();
    
    if (connections_.size() >= max_connections_) {
        throw std::runtime_error("Maximum connections limit reached");
    }
    
    if (connections_.find(id) != connections_.end()) {
        throw std::runtime_error("Connection ID already exists: " + std::to_string(id));
    }
    
    connections_[id] = connection;
    
    // 更新统计信息
    {
        std::lock_guard stats_lock(stats_mutex_);
        global_stats_.total_connections++;
        global_stats_.active_connections++;
        
        switch (connection->getType()) {
            case ConnectionType::TCP:
                global_stats_.tcp_connections++;
                break;
            case ConnectionType::WebSocket:
                global_stats_.websocket_connections++;
                break;
            case ConnectionType::HTTP:
                global_stats_.http_connections++;
                break;
        }
    }
    
    // 触发事件通知
    if (event_handler_) {
        event_handler_(id, ConnectionEvent::Connected);
    }
}

void ConnectionManager::removeConnection(ConnectionId connection_id) {
    std::unique_lock lock(connections_mutex_);
    auto it = connections_.find(connection_id);
    if (it == connections_.end()) {
        return; // 连接不存在
    }
    
    auto connection = it->second;
    connections_.erase(it);
    lock.unlock(); // 提前释放锁，避免在回调中死锁
    
    // 更新统计信息
    {
        std::lock_guard stats_lock(stats_mutex_);
        global_stats_.active_connections--;
        
        switch (connection->getType()) {
            case ConnectionType::TCP:
                global_stats_.tcp_connections--;
                break;
            case ConnectionType::WebSocket:
                global_stats_.websocket_connections--;
                break;
            case ConnectionType::HTTP:
                global_stats_.http_connections--;
                break;
        }
    }
    
    // 触发事件通知
    if (event_handler_) {
        event_handler_(connection_id, ConnectionEvent::Removed);
    }
}

void ConnectionManager::removeConnection(const Connection::Ptr& connection) {
    if (connection) {
        removeConnection(connection->getId());
    }
}

Connection::Ptr ConnectionManager::getConnection(ConnectionId connection_id) const {
    std::shared_lock lock(connections_mutex_);
    auto it = connections_.find(connection_id);
    return (it != connections_.end()) ? it->second : nullptr;
}

std::vector<Connection::Ptr> ConnectionManager::getConnectionsByType(ConnectionType type) const {
    std::vector<Connection::Ptr> result;
    std::shared_lock lock(connections_mutex_);
    
    for (const auto& [id, connection] : connections_) {
        if (connection->getType() == type) {
            result.push_back(connection);
        }
    }
    
    return result;
}

std::vector<Connection::Ptr> ConnectionManager::getConnectionsByState(ConnectionState state) const {
    std::vector<Connection::Ptr> result;
    std::shared_lock lock(connections_mutex_);
    
    for (const auto& [id, connection] : connections_) {
        if (connection->getState() == state) {
            result.push_back(connection);
        }
    }
    
    return result;
}

std::vector<Connection::Ptr> ConnectionManager::getAllConnections() const {
    std::vector<Connection::Ptr> result;
    std::shared_lock lock(connections_mutex_);
    
    result.reserve(connections_.size());
    for (const auto& [id, connection] : connections_) {
        result.push_back(connection);
    }
    
    return result;
}

size_t ConnectionManager::getConnectionCount() const {
    std::shared_lock lock(connections_mutex_);
    return connections_.size();
}

size_t ConnectionManager::getConnectionCount(ConnectionType type) const {
    std::shared_lock lock(connections_mutex_);
    size_t count = 0;
    
    for (const auto& [id, connection] : connections_) {
        if (connection->getType() == type) {
            count++;
        }
    }
    
    return count;
}

size_t ConnectionManager::getConnectionCount(ConnectionState state) const {
    std::shared_lock lock(connections_mutex_);
    size_t count = 0;
    
    for (const auto& [id, connection] : connections_) {
        if (connection->getState() == state) {
            count++;
        }
    }
    
    return count;
}

void ConnectionManager::closeAllConnections() {
    std::vector<Connection::Ptr> connections_to_close;
    
    {
        std::unique_lock lock(connections_mutex_);
        connections_to_close.reserve(connections_.size());
        
        for (auto& [id, connection] : connections_) {
            connections_to_close.push_back(connection);
        }
    }
    
    // 异步关闭所有连接
    for (auto& connection : connections_to_close) {
        if (connection && connection->isConnected()) {
            connection->close();
        }
    }
}

void ConnectionManager::closeConnectionsByType(ConnectionType type) {
    std::vector<Connection::Ptr> connections_to_close;
    
    {
        std::unique_lock lock(connections_mutex_);
        
        for (auto it = connections_.begin(); it != connections_.end();) {
            if (it->second->getType() == type) {
                connections_to_close.push_back(it->second);
                it = connections_.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    // 异步关闭指定类型的连接
    for (auto& connection : connections_to_close) {
        if (connection && connection->isConnected()) {
            connection->close();
        }
    }
    
    // 更新统计信息
    {
        std::lock_guard stats_lock(stats_mutex_);
        size_t closed_count = connections_to_close.size();
        global_stats_.active_connections -= closed_count;
        
        switch (type) {
            case ConnectionType::TCP:
                global_stats_.tcp_connections -= closed_count;
                break;
            case ConnectionType::WebSocket:
                global_stats_.websocket_connections -= closed_count;
                break;
            case ConnectionType::HTTP:
                global_stats_.http_connections -= closed_count;
                break;
        }
    }
}

void ConnectionManager::closeIdleConnections(std::chrono::seconds idle_timeout) {
    auto now = std::chrono::steady_clock::now();
    std::vector<ConnectionId> idle_connection_ids;

    {
        std::shared_lock lock(connections_mutex_);
        
        std::for_each(connections_.begin(), connections_.end(),
                    [now, idle_timeout, idle_connection_ids](const auto& pair) {
                        const auto& [id, connection] = pair;
                        if (connection
                             && !connection->isActive()
                             && std::chrono::duration_cast<std::chrono::seconds>(
                                now - connection->getStats().last_activity_time) >= idle_timeout) {
                            idle_connection_ids.push_back(id);
                        }
                    });
    }

    std::for_each(idle_connection_ids.begin(), idle_connection_ids.end(),
        [this](ConnectionId id) {
            auto connection = getConnection(id);
            if (connection && connection->isConnected()) {
                connection->close();
            }
        });

    // 事件通知
    if (!idle_connection_ids.empty() && event_handler_) {
        event_handler_(0, ConnectionEvent::Disconnected);
    }
}

void ConnectionManager::setMaxConnections(size_t max_connections) {
    if (max_connections == 0) {
        throw std::invalid_argument("Max connections must be greater than 0");
    }
    max_connections_ = max_connections;
}

void ConnectionManager::setIdleTimeout(std::chrono::seconds idle_timeout) {
    idle_timeout_ = idle_timeout;
}

void ConnectionManager::setEnableStatistics(bool enable) {
    enable_statistics_ = enable;
}

void ConnectionManager::setConnectionEventHandler(
    std::function<void(ConnectionId, ConnectionEvent)> handler) {
    event_handler_ = std::move(handler);
}

void ConnectionManager::updateGlobalStats() {
    if (!enable_statistics_) {
        return;
    }
    
    std::lock_guard stats_lock(stats_mutex_);
    
    // 重新计算统计信息
    global_stats_.active_connections = 0;
    global_stats_.tcp_connections = 0;
    global_stats_.websocket_connections = 0;
    global_stats_.http_connections = 0;
    global_stats_.total_bytes_sent = 0;
    global_stats_.total_bytes_received = 0;
    global_stats_.total_messages_sent = 0;
    global_stats_.total_messages_received = 0;
    
    std::shared_lock conn_lock(connections_mutex_);
    
    for (const auto& [id, connection] : connections_) {
        if (connection->isConnected()) {
            global_stats_.active_connections++;
            
            switch (connection->getType()) {
                case ConnectionType::TCP:
                    global_stats_.tcp_connections++;
                    break;
                case ConnectionType::WebSocket:
                    global_stats_.websocket_connections++;
                    break;
                case ConnectionType::HTTP:
                    global_stats_.http_connections++;
                    break;
            }
            
            if (enable_statistics_) {
                const auto& stats = connection->getStats();
                global_stats_.total_bytes_sent += stats.bytes_sent;
                global_stats_.total_bytes_received += stats.bytes_received;
                global_stats_.total_messages_sent += stats.messages_sent;
                global_stats_.total_messages_received += stats.messages_received;
            }
        }
    }
}

void ConnectionManager::cleanupClosedConnections() {
    std::vector<ConnectionId> closed_connection_ids;
    
    {
        std::shared_lock lock(connections_mutex_);
        
        for (const auto& [id, connection] : connections_) {
            if (connection && !connection->isConnected()) {
                closed_connection_ids.push_back(id);
            }
        }
    }
    
    // 移除已关闭的连接
    for (ConnectionId id : closed_connection_ids) {
        removeConnection(id);
    }
}

void ConnectionManager::checkIdleConnections() {
    closeIdleConnections(idle_timeout_);
}

} // namespace network