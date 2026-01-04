# IMServer 网络层技术文档

## 1. 模块概述

网络层是IMServer的通信基础，负责处理底层网络通信，支持多种协议（TCP、WebSocket、HTTP），提供高性能、可靠的连接管理和数据传输服务。该模块采用Boost::asio作为核心网络库，基于事件驱动的IO模型，能够高效处理大量并发连接。

## 2. 核心组件设计

### 2.1 统一连接管理架构

网络层采用了统一的连接管理架构，通过抽象化的Connection基类、统一的连接管理器（ConnectionManager）以及协议特定的连接实现，实现对TCP、WebSocket和HTTP连接的统一管理。

```
┌─────────────────────────────────────────────────────────────────┐
│                     网络层 (Network Layer)                      │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐ │
│  │ ConnectionManager│  │    TcpServer   │  │  Connection     │ │
│  │   (连接管理器)  │  │    (TCP服务器)  │  │  (基类/接口)    │ │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘ │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐ │
│  │ WebSocketServer │  │   HttpServer   │  │ IdGenerator     │ │
│  │ (WebSocket服务器)│  │  (HTTP服务器)  │  │ (ID生成器)      │ │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘ │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐ │
│  │  TcpConnection  │  │WebSocketConnection│  │  HttpConnection │ │
│  │  (TCP连接)      │  │  (WebSocket连接)  │  │  (HTTP连接)     │ │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 Connection 抽象基类

Connection是所有网络连接的抽象基类，定义了连接的基本接口和行为。

#### 2.2.1 核心接口

```cpp
class Connection : public std::enable_shared_from_this<Connection> {
public:
    // 基础信息获取
    ConnectionId getId() const;
    ConnectionType getType() const;
    ConnectionState getState() const;
    
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
    virtual bool isActive() const;
    virtual bool isOpen() const;
};
```

#### 2.2.2 关键特性

- **统一接口**: 所有连接类型提供一致的操作接口
- **状态管理**: 支持连接生命周期的完整状态管理
- **统计信息**: 内置连接统计功能，记录流量、消息数等
- **扩展数据**: 支持通过contexts_存储任意类型的扩展数据
- **线程安全**: 使用shared_mutex确保状态和统计数据的线程安全

### 2.3 ConnectionManager 连接管理器

ConnectionManager负责统一管理所有网络连接，提供连接的创建、查找、关闭等功能。

#### 2.3.1 核心功能

```cpp
class ConnectionManager {
public:
    // 连接注册和管理
    void addConnection(Connection::Ptr connection);
    void removeConnection(ConnectionId connection_id);
    void removeConnection(const Connection::Ptr& connection);
    
    // 连接查询
    Connection::Ptr getConnection(ConnectionId connection_id) const;
    std::vector<Connection::Ptr> getConnectionsByType(ConnectionType type) const;
    std::vector<Connection::Ptr> getConnectionsByState(ConnectionState state) const;
    std::vector<Connection::Ptr> getAllConnections() const;
    
    // 连接管理操作
    void closeAllConnections();
    void closeConnectionsByType(ConnectionType type);
    void closeIdleConnections(std::chrono::seconds idle_timeout);
};
```

#### 2.3.2 关键特性

- **统一管理**: 所有类型的连接通过同一个管理器管理
- **连接池**: 内置连接池，优化连接创建和销毁性能
- **状态监控**: 实时监控连接状态，支持按类型和状态查询
- **自动清理**: 定期清理空闲连接，优化资源使用
- **全局统计**: 提供全局连接统计信息

### 2.4 协议特定连接实现

#### 2.4.1 TcpConnection

TCP连接的具体实现，基于Boost.Asio的TCP socket。

```cpp
class TcpConnection : public Connection {
public:
    TcpConnection(ConnectionId id, boost::asio::ip::tcp::socket socket);
    
    // 连接生命周期管理
    void start() override;
    void close() override;
    void forceClose() override;
    
    // 数据传输
    void send(const std::vector<char>& data) override;
    void send(const std::string& data) override;
    void send(std::vector<char>&& data) override;
    
    // 远程端点信息
    boost::asio::ip::tcp::endpoint getRemoteEndpoint() const override;
    std::string getRemoteAddress() const override;
    uint16_t getRemotePort() const override;
    
    // 连接状态查询
    bool isConnected() const override;
};
```

**关键特性**:
- 异步读写操作
- 线程安全的发送队列
- 自动缓冲区管理
- 完整的错误处理机制

#### 2.4.2 WebSocketConnection

WebSocket连接的具体实现，基于Boost.Beast的WebSocket库。

```cpp
class WebSocketConnection : public Connection {
public:
    using Ptr = std::shared_ptr<WebSocketConnection>;

private:
    websocket::stream<asio::ip::tcp::socket> ws_;
    beast::flat_buffer buffer_;
    std::queue<std::vector<char>> write_queue_;
    std::mutex write_mutex_;
    std::atomic<bool> running_;

public:
    WebSocketConnection(ConnectionId id, boost::asio::ip::tcp::socket socket);
    ~WebSocketConnection();

    void start() override;
    void close() override;
    void forceClose() override;
    void send(const std::vector<char>& data) override;
    void send(const std::string& data) override;
    void send(std::vector<char>&& data) override;

    boost::asio::ip::tcp::endpoint getRemoteEndpoint() const override;
    std::string getRemoteAddress() const override;
    uint16_t getRemotePort() const override;
    bool isConnected() const override;

    // 检查连接是否打开
    bool is_open() const {
        return ws_.next_layer().is_open();
    }
};
```

**关键特性**:
- 支持WebSocket协议的完整功能
- 自动处理握手过程
- 支持文本和二进制消息
- 内置消息队列，线程安全
- 异步读写操作
- 完整的生命周期管理
- 连接状态查询方法

#### 2.4.3 HttpConnection

HTTP连接的具体实现，基于Boost.Beast的HTTP库。

```cpp
class HttpConnection : public network::Connection {
public:
    using Ptr = std::shared_ptr<HttpConnection>;

private:
    boost::asio::ip::tcp::socket socket_;
    beast::flat_buffer buffer_;
    HttpRequest request_;
    HttpResponse response_;
    HttpServer* server_;
    bool is_writing_;

    // 超时定时器
    net::steady_timer timeout_timer_;

public:
    HttpConnection(ConnectionId id, boost::asio::ip::tcp::socket socket, HttpServer* server);
    ~HttpConnection();

    void start() override;
    void close() override;
    void forceClose() override;

    /**
     * @brief 发送HTTP响应
     * 
     * 该函数用于发送HTTP响应到客户端。它会将响应转换为字符串并发送。
     * 
     * @param response 要发送的HTTP响应对象
     */
    void sendResponse(const HttpResponse& response);
    void send(const std::vector<char>& data) override;
    void send(const std::string& data) override;
    void send(std::vector<char>&& data) override;

    std::string getRemoteAddress() const override;
    uint16_t getRemotePort() const override;
    boost::asio::ip::tcp::endpoint getRemoteEndpoint() const override;

    bool isConnected() const override;
};
```

**关键特性**:
- 支持HTTP/1.1协议
- 自动解析HTTP请求
- 支持多种HTTP方法（GET, POST, PUT, DELETE等）
- 支持静态文件服务
- 支持CORS跨域请求
- 内置超时定时器
- 异步读写操作
- 完整的错误处理机制
- 支持HTTP响应构造和发送
- 与HttpServer紧密集成，支持路由处理

### 2.5 服务器实现

#### 2.5.1 TcpServer

TCP服务器，负责监听和接受TCP连接。

```cpp
class TcpServer : public std::enable_shared_from_this<TcpServer> {
private:
    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    ConnectionManager& connection_manager_;
    std::atomic<bool> running_;
    
public:
    TcpServer(boost::asio::io_context& io_context, ConnectionManager& connection_manager, const std::string& address, uint16_t port);
    
    void start();
    void stop();
    bool isRunning() const;
    
private:
    void doAccept();
    void handleAccept(boost::system::error_code ec, boost::asio::ip::tcp::socket socket);
};
```

#### 2.5.2 WebSocketServer

WebSocket服务器，负责监听和接受WebSocket连接。

```cpp
class WebSocketServer : public std::enable_shared_from_this<WebSocketServer> {
private:
    asio::io_context& io_context_;
    asio::ip::tcp::acceptor acceptor_;
    ConnectionManager& connection_manager_;
    std::atomic<bool> running_;
    
public:
    WebSocketServer(asio::io_context& io_context, ConnectionManager& connection_manager, const std::string& address, uint16_t port);
    
    void start();
    void stop();
    bool isRunning() const;
    
private:
    void doAccept();
    void handleAccept(beast::error_code ec, asio::ip::tcp::socket socket);
};
```

#### 2.5.3 HttpServer

HTTP服务器，负责处理HTTP请求，基于Boost.Beast和Boost.Asio库。

```cpp
class HttpServer {
public:
    using address = net::ip::address;

private:
    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    ConnectionManager& connection_manager_;

    bool running_;

    // 优化的路由表结构
    // 第一层：按HTTP方法分组
    // 第二层：按路径存储处理器（使用unordered_map实现O(1)查找）
    using PathHandlerMap = std::unordered_map<std::string, HttpRequestHandler>;
    std::unordered_map<std::string, PathHandlerMap> route_tables_;

    // 静态文件目录
    std::string static_file_directory_;

    // CORS设置
    bool cors_enabled_;
    std::string cors_origin_;

public:
    explicit HttpServer(net::io_context& io_context, ConnectionManager& connection_manager);
    ~HttpServer();

    void start(const address& addr, uint16_t port);
    void stop();
    bool isRunning() const;

    void registerRoute(const std::string& method, const std::string& path, HttpRequestHandler handler);

    /**
     * @brief 注册路由
     * 
     * 该函数用于注册一个路由，当客户端发送指定方法的请求到指定路径时，会调用该路由的处理器函数。
     * 
     * @param path HTTP请求路径
     * @param handler 处理请求的处理器函数
     */
    void get(const std::string& path, HttpRequestHandler handler);
    void post(const std::string& path, HttpRequestHandler handler);
    void put(const std::string& path, HttpRequestHandler handler);
    void del(const std::string& path, HttpRequestHandler handler);

    /**
     * @brief 设置静态文件目录
     * 
     * 该函数用于设置服务器的静态文件目录。当客户端请求静态文件时，服务器会从该目录中读取文件内容并发送给客户端。
     * 
     * @param directory 静态文件目录的路径
     */
    void setStaticFileDirectory(const std::string& directory);
    std::string &getStaticFileDirectory();

    /**
     * @brief 启用跨域资源共享（CORS）
     * 
     * 该函数用于启用服务器的跨域资源共享功能。当客户端从不同域名或端口请求资源时，
     * 服务器会根据设置的允许来源（origin）发送适当的CORS响应头。
     * 
     * @param origin 允许的来源域名，默认值为"*"，表示允许所有来源
     */
    void enableCORS(const std::string &origin = "*");
    bool isCORSEnabled() const;
    std::string getCORSOrigin() const;

    HttpRequestHandler findHandlerInTable(const std::string& method, const std::string& path);

private:
    void doAccept();
    void addRouteToTable(const std::string& method, const std::string& path, HttpRequestHandler handler);
};
```

## 3. 核心工具类

### 3.1 IdGenerator

全局唯一ID生成器，用于生成连接ID、用户ID、消息ID等。

```cpp
class IdGenerator {
public:
    using ConnectionId = uint64_t;
    using UserId = uint64_t;
    using MessageId = uint64_t;
    using SessionId = uint64_t;
    using TimestampId = uint64_t;
    
    // 单例模式
    static IdGenerator& getInstance();
    
    // ID生成方法
    ConnectionId generateConnectionId();
    UserId generateUserId();
    MessageId generateMessageId();
    SessionId generateSessionId();
    TimestampId generateTimestampId(const std::string& prefix);
    std::string generateUuid();
    std::string generateShortId(size_t length = 8);
};
```

**关键特性**:
- 线程安全的ID生成
- 支持多种ID类型
- 基于时间戳和进程ID的唯一ID
- 支持UUID和短ID生成

## 4. 数据流和交互流程

### 4.1 客户端连接流程

1. 客户端发起TCP/WebSocket连接请求
2. 服务器接受连接请求
3. 调用IdGenerator生成唯一连接ID
4. 创建对应类型的Connection对象
5. 将Connection对象添加到ConnectionManager管理
6. 调用Connection::start()开始处理数据传输
7. 触发连接建立事件

### 4.2 消息接收流程

1. 连接对象接收到网络数据
2. 更新连接统计信息
3. 调用消息处理回调函数
4. 将数据传递给上层模块处理
5. 清理已处理的数据

### 4.3 消息发送流程

1. 上层模块调用Connection::send()发送数据
2. 将数据添加到发送队列
3. 异步写入网络
4. 处理发送完成事件
5. 更新连接统计信息

### 4.4 客户端断开连接流程

1. 检测到客户端断开连接
2. 更新连接状态为Disconnected
3. 调用连接关闭回调函数
4. 从ConnectionManager中移除连接
5. 清理连接资源

## 5. 技术特点和优势

### 5.1 高性能

- **异步IO**: 基于Boost.Asio的异步IO模型，高效处理大量并发连接
- **零拷贝**: 优化数据传输，减少内存拷贝
- **对象池**: 连接对象复用，减少动态内存分配开销
- **线程安全**: 采用细粒度锁，减少锁竞争

### 5.2 可靠性

- **完善的错误处理**: 全面的错误检测和恢复机制
- **优雅关闭**: 支持连接的优雅关闭，确保数据完整传输
- **超时机制**: 内置超时处理，避免资源泄露
- **连接监控**: 实时监控连接状态，自动清理异常连接

### 5.3 可扩展性

- **模块化设计**: 清晰的模块划分，易于扩展新功能
- **协议扩展**: 支持轻松添加新的协议支持
- **插件化架构**: 支持通过插件扩展功能
- **配置驱动**: 灵活的配置选项，支持动态调整

### 5.4 易用性

- **简洁API**: 统一的接口设计，易于使用
- **事件驱动**: 基于回调函数的事件处理机制
- **全面文档**: 详细的技术文档和使用示例
- **丰富工具**: 提供丰富的辅助工具类

## 6. 使用示例

### 6.1 基本使用示例

```cpp
#include "network/ConnectionManager.h"
#include "network/TcpServer.h"
#include "network/WebSocketServer.h"
#include "network/HttpServer.h"
#include "tool/IdGenerator.h"

int main() {
    // 创建IO上下文
    boost::asio::io_context io_context;
    
    // 创建连接管理器
    ConnectionManager connection_manager;
    
    // 初始化连接管理器的清理定时器
    connection_manager.initializeCleanupTimer(io_context);
    
    // 创建服务器
    TcpServer tcp_server(io_context, connection_manager, "0.0.0.0", 8000);
    WebSocketServer ws_server(io_context, connection_manager, "0.0.0.0", 8080);
    HttpServer http_server(io_context, connection_manager);
    
    // 配置HTTP服务器
    http_server.registerRoute("GET", "/", [](const HttpRequest& req, HttpResponse& resp) {
        resp.result(http::status::ok);
        resp.body() = "Hello, IMServer!";
    });
    
    // 启动服务器
    tcp_server.start();
    ws_server.start();
    http_server.start(boost::asio::ip::address::from_string("0.0.0.0"), 8081);
    
    // 启动IO上下文
    io_context.run();
    
    return 0;
}
```

### 6.2 连接事件处理

```cpp
// 创建连接管理器
ConnectionManager connection_manager;

// 设置连接事件处理器
connection_manager.setConnectionEventHandler([](ConnectionId conn_id, ConnectionEvent event) {
    std::cout << "Connection " << conn_id << " event: " << connectionEventToString(event) << std::endl;
});

// 创建TCP服务器
TcpServer tcp_server(io_context, connection_manager, "0.0.0.0", 8000);

// 配置TCP服务器
tcp_server.start();
```

### 6.3 连接统计和监控

```cpp
// 获取全局统计信息
auto stats = connection_manager.getGlobalStats();
std::cout << "总连接数: " << stats.total_connections << std::endl;
std::cout << "活跃连接数: " << stats.active_connections << std::endl;
std::cout << "TCP连接数: " << stats.tcp_connections << std::endl;
std::cout << "WebSocket连接数: " << stats.websocket_connections << std::endl;
std::cout << "HTTP连接数: " << stats.http_connections << std::endl;
std::cout << "总发送字节数: " << stats.total_bytes_sent << std::endl;
std::cout << "总接收字节数: " << stats.total_bytes_received << std::endl;
```

## 7. 性能优化策略

### 7.1 内存管理优化

- **对象池**: 连接对象复用，减少动态内存分配
- **缓冲区复用**: 读写缓冲区复用，减少内存拷贝
- **智能指针**: 自动内存管理，避免内存泄漏
- **内存对齐**: 优化数据结构内存布局

### 7.2 并发性能优化

- **读写分离**: 使用shared_mutex实现读写分离
- **批量操作**: 支持批量发送和接收数据
- **无锁设计**: 减少锁竞争
- **线程局部存储**: 减少线程间数据共享

### 7.3 网络性能优化

- **TCP_NODELAY**: 禁用Nagle算法，减少延迟
- **SO_REUSEADDR**: 支持地址重用
- **SO_KEEPALIVE**: 启用TCP保活机制
- **调整缓冲区大小**: 优化TCP读写缓冲区大小

## 8. 错误处理和恢复

### 8.1 异常处理

- **RAII模式**: 使用RAII确保资源正确释放
- **异常安全**: 所有公共接口提供异常安全保证
- **日志记录**: 详细的错误日志记录

### 8.2 故障恢复

- **自动重连**: 支持客户端自动重连
- **连接健康检查**: 定期检查连接健康状态
- **熔断器模式**: 在异常情况下自动断开问题连接
- **限流机制**: 防止过载

## 9. 未来扩展计划

### 9.1 功能扩展

- **QUIC协议支持**: 添加QUIC协议支持，提供更高效的传输
- **gRPC支持**: 添加gRPC支持，方便服务间通信
- **TLS/SSL加密**: 支持TLS/SSL加密通信
- **负载均衡**: 添加内置负载均衡功能

### 9.2 性能优化

- **零拷贝技术**: 进一步优化数据传输，减少内存拷贝
- **硬件加速**: 支持硬件加速技术
- **自适应调整**: 根据负载自动调整配置

### 9.3 监控和可观测性

- **分布式追踪**: 添加分布式追踪支持
- **实时监控**: 实时监控系统性能指标
- **告警机制**: 支持基于阈值的告警
- **日志增强**: 提供更丰富的日志信息

## 10. 总结

网络层是IMServer的重要组成部分，提供了高性能、可靠的网络通信服务。该模块基于Boost.Asio库，采用事件驱动的IO模型，支持多种协议，能够高效处理大量并发连接。

通过统一的连接管理架构，网络层实现了对TCP、WebSocket和HTTP连接的统一管理，提供了简洁易用的API，同时保持了良好的性能和可靠性。

未来，我们将继续优化网络层的性能，扩展新的功能，提高系统的可扩展性和可靠性，为IMServer提供更强大的网络通信支持。