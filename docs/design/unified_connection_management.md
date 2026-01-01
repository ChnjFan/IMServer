# IMServer 统一连接管理系统设计

## 1. 设计概述

为了解决当前网络层存在的连接管理分散、重复代码过多、维护困难等问题，本文提出了一种全新的统一连接管理系统。该系统通过抽象化的Connection基类、统一的连接管理器（ConnectionManager）以及协议特定的连接实现，实现对TCP、WebSocket和HTTP连接的统一管理。

### 1.1 设计目标
- **统一管理**: 对所有类型的网络连接提供统一的管理接口
- **高性能**: 优化连接创建、销毁和数据传输性能
- **可扩展性**: 易于添加新的协议支持
- **可维护性**: 减少代码重复，提高代码复用性
- **稳定性**: 提供完善的错误处理和资源管理

### 1.2 核心特性
- 抽象化的连接基类设计
- 统一的连接管理器
- 协议特定的连接实现
- 连接生命周期管理
- 连接状态监控和统计
- 线程安全的操作
- 优雅的关闭和清理机制

## 2. 系统架构设计

### 2.1 整体架构

```
┌─────────────────────────────────────────────────────────────────┐
│                        应用层 (Application Layer)               │
└─────────────────────────────────────────────────────────────────┘
                                ↑ ↓
┌─────────────────────────────────────────────────────────────────┐
│                   统一连接管理层 (Unified Connection Layer)     │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐ │
│  │ ConnectionManager│  │  ConnectionPool │  │  Connection     │ │
│  │   (管理器)      │  │    (连接池)     │  │  (基类/接口)    │ │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
                                ↑ ↓
┌─────────────────────────────────────────────────────────────────┐
│                   协议适配层 (Protocol Adapter Layer)           │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐ │
│  │  TcpConnection  │  │ WebSocketConn   │  │  HttpConnection │ │
│  │  (TCP连接)      │  │  (WebSocket)    │  │  (HTTP连接)     │ │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
                                ↑ ↓
┌─────────────────────────────────────────────────────────────────┐
│                     网络层 (Network Layer)                      │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐ │
│  │  TcpServer      │  │WebSocketServer  │  │  HttpServer     │ │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 核心组件

#### 2.2.1 Connection (抽象基类)
提供所有连接类型的通用接口，定义连接的基本操作和状态管理。

#### 2.2.2 Protocol Connections (协议连接)
- **TcpConnection**: TCP协议的连接实现
- **WebSocketConnection**: WebSocket协议的连接实现
- **HttpConnection**: HTTP协议的连接实现

#### 2.2.3 ConnectionManager (连接管理器)
统一管理所有连接，提供连接的生命周期管理、状态监控和统计功能。

#### 2.2.4 ConnectionPool (连接池)
提供连接的重用机制，减少连接创建和销毁的开销。

## 3. 详细设计

### 3.1 Connection 抽象基类设计

```cpp
namespace imserver {
namespace network {

// 连接状态枚举
enum class ConnectionState {
    Disconnected,    // 未连接
    Connecting,      // 连接中
    Connected,       // 已连接
    Disconnecting,   // 断开中
    Error           // 错误状态
};

// 连接类型枚举
enum class ConnectionType {
    TCP,
    WebSocket,
    HTTP
};

// 连接统计信息
struct ConnectionStats {
    uint64_t bytes_sent = 0;           // 发送字节数
    uint64_t bytes_received = 0;       // 接收字节数
    uint64_t messages_sent = 0;        // 发送消息数
    uint64_t messages_received = 0;    // 接收消息数
    std::chrono::steady_clock::time_point connected_time; // 连接时间
    std::chrono::steady_clock::time_point last_activity_time; // 最后活动时间
};

// 连接ID类型
using ConnectionId = uint64_t;

// 消息处理回调函数
using MessageHandler = std::function<void(ConnectionId, const std::vector<char>&)>;

// 连接状态变更回调函数
using StateChangeHandler = std::function<void(ConnectionId, ConnectionState, ConnectionState)>;

// 连接关闭回调函数
using CloseHandler = std::function<void(ConnectionId, const boost::system::error_code&)>;

// 连接抽象基类
class Connection : public std::enable_shared_from_this<Connection> {
public:
    using Ptr = std::shared_ptr<Connection>;
    using WeakPtr = std::weak_ptr<Connection>;

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
    
    // 回调触发辅助方法
    void triggerMessageHandler(const std::vector<char>& data);
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

protected:
    ConnectionId connection_id_;
    ConnectionType connection_type_;
    ConnectionState state_;
    ConnectionStats stats_;
    
    // 回调函数
    MessageHandler message_handler_;
    StateChangeHandler state_change_handler_;
    CloseHandler close_handler_;
    
    // 扩展数据存储
    std::unordered_map<std::string, std::any> contexts_;
    
    // 互斥锁（保护状态和统计数据）
    mutable std::shared_mutex state_mutex_;
};

} // namespace network
} // namespace imserver
```

### 3.2 协议特定连接实现

#### 3.2.1 TcpConnection 实现

```cpp
namespace imserver {
namespace network {

class TcpConnection : public Connection {
public:
    TcpConnection(ConnectionId id, boost::asio::ip::tcp::socket socket);
    ~TcpConnection() override;

    // 重写基类方法
    void start() override;
    void close() override;
    void forceClose() override;
    
    bool isConnected() const override;
    
    // TCP特定方法
    boost::asio::ip::tcp::socket& getSocket() { return socket_; }
    const boost::asio::ip::tcp::socket& getSocket() const { return socket_; }
    
    // 端点信息
    boost::asio::ip::tcp::endpoint getRemoteEndpoint() const override;
    std::string getRemoteAddress() const override;
    uint16_t getRemotePort() const override;

protected:
    // 异步操作
    void doRead();
    void doWrite();
    void handleRead(const boost::system::error_code& error, size_t bytes_transferred);
    void handleWrite(const boost::system::error_code& error, size_t bytes_transferred);

private:
    boost::asio::ip::tcp::socket socket_;
    std::vector<char> read_buffer_;
    std::vector<char> write_buffer_;
    std::mutex write_mutex_;
    std::atomic<bool> closing_;
};

} // namespace network
} // namespace imserver
```

#### 3.2.2 WebSocketConnection 实现

```cpp
namespace imserver {
namespace network {

class WebSocketConnection : public Connection {
public:
    WebSocketConnection(ConnectionId id, boost::asio::ip::tcp::socket socket);
    ~WebSocketConnection() override;

    // 重写基类方法
    void start() override;
    void close() override;
    void forceClose() override;
    
    bool isConnected() const override;
    
    // WebSocket特定方法
    void sendText(const std::string& message);
    void sendBinary(const std::vector<char>& data);
    void sendPing();
    void sendPong();
    
    // 协议信息
    std::string getSubProtocol() const;
    std::string getExtensions() const;

protected:
    // 异步操作
    void doHandshake();
    void doRead();
    void doWrite();
    void handleHandshake(const boost::system::error_code& error);
    void handleRead(const boost::system::error_code& error, size_t bytes_transferred);
    void handleWrite(const boost::system::error_code& error, size_t bytes_transferred);

private:
    websocket::stream<tcp::socket> ws_;
    boost::beast::flat_buffer read_buffer_;
    std::queue<std::vector<char>> write_queue_;
    std::mutex write_mutex_;
    std::atomic<bool> handshaking_;
    std::atomic<bool> closing_;
};

} // namespace network
} // namespace imserver
```

#### 3.2.3 HttpConnection 实现

```cpp
namespace imserver {
namespace network {

// HTTP请求结构
struct HttpRequest {
    std::string method;
    std::string target;
    std::string version;
    boost::beast::http::request<boost::beast::http::string_body> request;
    boost::asio::ip::tcp::endpoint remote_endpoint;
    std::chrono::steady_clock::time_point received_time;
};

// HTTP响应结构
struct HttpResponse {
    boost::beast::http::response<boost::beast::http::string_body> response;
    std::chrono::steady_clock::time_point sent_time;
};

// HTTP请求处理回调
using HttpRequestHandler = std::function<void(ConnectionId, const HttpRequest&, HttpResponse&)>;

class HttpConnection : public Connection {
public:
    HttpConnection(ConnectionId id, boost::asio::ip::tcp::socket socket);
    ~HttpConnection() override;

    // 重写基类方法
    void start() override;
    void close() override;
    void forceClose() override;
    
    bool isConnected() const override;
    
    // HTTP特定方法
    void sendResponse(const HttpResponse& response);
    void sendRedirect(const std::string& location, int status_code = 302);
    void sendError(int status_code, const std::string& message);
    
    // 静态资源服务
    void serveStaticFile(const std::string& file_path);
    void setStaticFileRoot(const std::string& root_path);

protected:
    // 异步操作
    void doRead();
    void doWrite();
    void handleRead(const boost::system::error_code& error, size_t bytes_transferred);
    void handleWrite(const boost::system::error_code& error, size_t bytes_transferred);
    
    // HTTP协议处理
    void handleHttpRequest(const HttpRequest& request);
    HttpResponse createDefaultResponse(int status_code);

private:
    boost::beast::tcp_stream stream_;
    boost::beast::flat_buffer read_buffer_;
    std::vector<char> write_buffer_;
    std::mutex write_mutex_;
    std::atomic<bool> closing_;
    
    // 静态文件服务配置
    std::string static_file_root_;
    std::unordered_map<std::string, std::string> mime_types_;
    
    // HTTP状态
    bool keep_alive_;
    std::chrono::steady_clock::time_point last_request_time_;
};

} // namespace network
} // namespace imserver
```

### 3.3 连接管理器设计

```cpp
namespace imserver {
namespace network {

class ConnectionManager {
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
    
    // 全局统计信息
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
    
    const GlobalStats& getGlobalStats() const { return global_stats_; }
    
    // 配置管理
    void setMaxConnections(size_t max_connections);
    void setIdleTimeout(std::chrono::seconds idle_timeout);
    void setEnableStatistics(bool enable);
    
    // 事件通知
    void setConnectionEventHandler(std::function<void(ConnectionId, const std::string&)> handler);

private:
    // 内部管理方法
    void updateGlobalStats();
    void cleanupClosedConnections();
    void checkIdleConnections();
    
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
    std::function<void(ConnectionId, const std::string&)> event_handler_;
    
    // 清理定时器
    std::unique_ptr<boost::asio::steady_timer> cleanup_timer_;
};

} // namespace network
} // namespace imserver
```

### 3.4 连接池设计

```cpp
namespace imserver {
namespace network {

template<typename T>
class ObjectPool {
public:
    ObjectPool(size_t initial_size = 10, size_t max_size = 100);
    ~ObjectPool();

    // 获取对象（如果没有可用对象则创建新的）
    template<typename... Args>
    std::shared_ptr<T> acquire(Args&&... args);

    // 归还对象到池中
    void release(std::shared_ptr<T> object);

    // 清理池中所有对象
    void clear();

    // 获取池统计信息
    size_t getTotalObjects() const { return total_objects_; }
    size_t getAvailableObjects() const { return available_objects_.size(); }
    size_t getAcquiredObjects() const { return total_objects_ - available_objects_.size(); }

private:
    void expandPool(size_t count);
    void shrinkPool();

private:
    std::queue<std::shared_ptr<T>> available_objects_;
    size_t max_size_;
    std::atomic<size_t> total_objects_;
    std::mutex pool_mutex_;
};

// 连接池特化
using ConnectionPool = ObjectPool<Connection>;

} // namespace network
} // namespace imserver
```

### 3.5 服务器集成

#### 3.5.1 通用服务器基类

```cpp
namespace imserver {
namespace network {

template<typename ConnectionType>
class BaseServer {
public:
    BaseServer(boost::asio::io_context& io_context, 
               const std::string& address, 
               uint16_t port);
    virtual ~BaseServer() = default;

    // 禁止拷贝和赋值
    BaseServer(const BaseServer&) = delete;
    BaseServer& operator=(const BaseServer&) = delete;

    // 生命周期管理
    virtual void start();
    virtual void stop();
    virtual bool isRunning() const { return running_.load(); }

    // 连接管理
    void setConnectionManager(ConnectionManager* manager) { 
        connection_manager_ = manager; 
    }

    // 配置选项
    void setMaxConnections(size_t max_connections) { 
        max_connections_ = max_connections; 
    }
    void setAcceptTimeout(std::chrono::seconds timeout) { 
        accept_timeout_ = timeout; 
    }

protected:
    // 模板方法模式，由子类实现具体创建逻辑
    virtual std::shared_ptr<ConnectionType> createConnection(
        boost::asio::ip::tcp::socket socket) = 0;
    virtual void handleNewConnection(ConnectionId connection_id) = 0;

    // 异步接受连接
    void doAccept();
    void handleAccept(const boost::system::error_code& error, 
                     boost::asio::ip::tcp::socket socket);

protected:
    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::atomic<bool> running_;
    
    // 连接管理
    ConnectionManager* connection_manager_ = nullptr;
    size_t max_connections_ = 1000;
    std::chrono::seconds accept_timeout_{30};
    
    // 统计信息
    std::atomic<uint64_t> total_connections_{0};
    std::atomic<uint64_t> active_connections_{0};
};

// TCP服务器特化
using TcpServer = BaseServer<TcpConnection>;

// WebSocket服务器特化
using WebSocketServer = BaseServer<WebSocketConnection>;

// HTTP服务器特化
using HttpServer = BaseServer<HttpConnection>;

} // namespace network
} // namespace imserver
```

## 4. 使用示例

### 4.1 基本使用

```cpp
#include "network/ConnectionManager.h"
#include "network/TcpServer.h"
#include "network/WebSocketServer.h"
#include "network/HttpServer.h"

int main() {
    // 创建事件循环
    boost::asio::io_context io_context;
    
    // 创建连接管理器
    imserver::network::ConnectionManager connection_manager;
    
    // 创建TCP服务器
    auto tcp_server = std::make_shared<imserver::network::TcpServer>(
        io_context, "0.0.0.0", 8000);
    tcp_server->setConnectionManager(&connection_manager);
    
    // 创建WebSocket服务器
    auto ws_server = std::make_shared<imserver::network::WebSocketServer>(
        io_context, "0.0.0.0", 8080);
    ws_server->setConnectionManager(&connection_manager);
    
    // 创建HTTP服务器
    auto http_server = std::make_shared<imserver::network::HttpServer>(
        io_context, "0.0.0.0", 8081);
    http_server->setConnectionManager(&connection_manager);
    
    // 启动服务器
    tcp_server->start();
    ws_server->start();
    http_server->start();
    
    // 启动事件循环
    io_context.run();
    
    return 0;
}
```

### 4.2 连接事件处理

```cpp
// 设置连接事件处理器
connection_manager.setConnectionEventHandler(
    [](imserver::network::ConnectionId connection_id, const std::string& event) {
        std::cout << "Connection " << connection_id << " event: " << event << std::endl;
    }
);

// 设置消息处理器
tcp_server->setMessageHandler(
    [](imserver::network::ConnectionId connection_id, const std::vector<char>& data) {
        std::cout << "Received message from connection " << connection_id 
                  << ": " << std::string(data.begin(), data.end()) << std::endl;
        
        // 回复消息
        auto connection = connection_manager.getConnection(connection_id);
        if (connection && connection->isConnected()) {
            connection->send("Echo: " + std::string(data.begin(), data.end()));
        }
    }
);
```

### 4.3 连接统计和监控

```cpp
// 获取全局统计信息
auto stats = connection_manager.getGlobalStats();
std::cout << "Total connections: " << stats.total_connections << std::endl;
std::cout << "Active connections: " << stats.active_connections << std::endl;
std::cout << "TCP connections: " << stats.tcp_connections << std::endl;
std::cout << "WebSocket connections: " << stats.websocket_connections << std::endl;
std::cout << "HTTP connections: " << stats.http_connections << std::endl;

// 获取特定连接的详细信息
auto connection = connection_manager.getConnection(connection_id);
if (connection) {
    auto& conn_stats = connection->getStats();
    std::cout << "Connection " << connection_id << " stats:" << std::endl;
    std::cout << "  Type: " << static_cast<int>(connection->getType()) << std::endl;
    std::cout << "  State: " << static_cast<int>(connection->getState()) << std::endl;
    std::cout << "  Bytes sent: " << conn_stats.bytes_sent << std::endl;
    std::cout << "  Bytes received: " << conn_stats.bytes_received << std::endl;
    std::cout << "  Messages sent: " << conn_stats.messages_sent << std::endl;
    std::cout << "  Messages received: " << conn_stats.messages_received << std::endl;
}
```

## 5. 性能优化策略

### 5.1 内存管理优化
- **对象池**: 减少连接对象的动态分配和释放
- **缓冲区复用**: 减少读写缓冲区的内存分配
- **智能指针**: 自动内存管理，避免内存泄漏

### 5.2 并发性能优化
- **读写分离**: 读写操作使用不同的锁策略
- **批量操作**: 支持批量发送和接收数据
- **异步IO**: 完全异步的IO操作，避免阻塞

### 5.3 连接管理优化
- **连接池**: 重用连接对象，减少创建销毁开销
- **状态缓存**: 缓存连接状态，避免频繁的系统调用
- **懒加载**: 按需加载连接统计信息

## 6. 错误处理和恢复

### 6.1 异常处理策略
- **RAII模式**: 使用RAII确保资源正确释放
- **异常安全**: 所有公共接口提供异常安全保证
- **日志记录**: 详细的错误日志记录

### 6.2 故障恢复机制
- **自动重连**: 支持客户端自动重连
- **连接健康检查**: 定期检查连接健康状态
- **熔断器模式**: 在异常情况下自动断开问题连接

## 7. 扩展性设计

### 7.1 新协议支持
- **插件化设计**: 支持动态加载新协议
- **协议工厂**: 统一的协议对象创建工厂
- **配置驱动**: 通过配置文件启用/禁用协议

### 7.2 功能扩展点
- **中间件支持**: 支持连接级别的中间件
- **过滤器**: 支持数据过滤和转换
- **监控钩子**: 支持自定义监控指标

## 8. 总结

统一连接管理系统通过抽象化的设计和统一的接口，为IMServer提供了高效、可靠、可扩展的网络连接管理能力。该设计不仅解决了当前网络层存在的问题，还为未来的扩展和维护奠定了坚实的基础。

### 8.1 主要优势
- **统一管理**: 所有连接类型统一管理，简化使用和维护
- **高性能**: 通过对象池、异步IO等技术优化性能
- **高可靠**: 完善的错误处理和故障恢复机制
- **易扩展**: 支持新协议的轻松添加和配置
- **可监控**: 丰富的统计信息和监控能力

### 8.2 实施建议
1. **渐进式迁移**: 从现有系统逐步迁移到新系统
2. **充分测试**: 对所有组件进行全面的单元测试和集成测试
3. **性能基准**: 建立性能基准，确保新系统的性能优势
4. **文档完善**: 完善API文档和使用指南
5. **监控部署**: 部署监控和告警系统，确保系统稳定运行

通过这个统一连接管理系统，IMServer将能够更好地支持大规模并发连接，提供更稳定的网络通信服务。