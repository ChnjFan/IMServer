# IMServer 网络层技术文档

## 1. 模块概述

网络层是IMServer的通信基础，负责处理底层网络通信，支持多种协议（WebSocket、TCP），提供高性能、可靠的连接管理和数据传输服务。该模块采用Boost::asio作为核心网络库，基于事件驱动的IO模型，能够高效处理大量并发连接。

## 2. 核心组件设计

### 2.1 EventLoop

#### 2.1.1 功能描述
事件循环是网络层的核心组件，负责处理网络事件、定时器事件等各种IO事件，提供高效的事件处理机制。

#### 2.1.2 实现细节
- 基于Boost::asio的io_context实现
- 提供事件循环的启动、停止和运行状态管理
- 支持定时器功能
- 支持多线程事件处理

```cpp
class EventLoop {
private:
    boost::asio::io_context io_context_;
    std::unique_ptr<boost::asio::io_context::work> work_;
    std::thread thread_;
    std::atomic<bool> running_;

public:
    EventLoop();
    ~EventLoop();
    
    // 启动事件循环
    void start();
    
    // 停止事件循环
    void stop();
    
    // 获取io_context对象
    boost::asio::io_context& getIoContext();
    
    // 运行状态检查
    bool isRunning() const;
    
    // 延迟执行任务
    template<typename CompletionHandler>
    void post(CompletionHandler handler);
    
    // 定时执行任务
    template<typename Duration, typename CompletionHandler>
    void runAfter(Duration duration, CompletionHandler handler);
};
```

### 2.2 Connection

#### 2.2.1 功能描述
连接管理类负责管理单个网络连接的生命周期，包括连接的建立、数据的发送和接收、连接的关闭等。

#### 2.2.2 数据结构
```cpp
class Connection : public std::enable_shared_from_this<Connection> {
public:
    using Ptr = std::shared_ptr<Connection>;
    using MessageHandler = std::function<void(const Ptr&, const std::vector<char>&)>;
    using CloseHandler = std::function<void(const Ptr&)>;
    
private:
    boost::asio::ip::tcp::socket socket_;
    std::vector<char> read_buffer_;
    std::vector<char> write_buffer_;
    std::mutex write_mutex_;
    MessageHandler message_handler_;
    CloseHandler close_handler_;
    bool closed_;

public:
    Connection(boost::asio::ip::tcp::socket socket);
    ~Connection();
    
    // 启动连接
    void start();
    
    // 关闭连接
    void close();
    
    // 发送数据
    void send(const std::vector<char>& data);
    
    // 获取远程端点信息
    boost::asio::ip::tcp::endpoint getRemoteEndpoint() const;
    
    // 设置消息处理回调
    void setMessageHandler(MessageHandler handler);
    
    // 设置关闭处理回调
    void setCloseHandler(CloseHandler handler);
    
    // 连接状态检查
    bool isClosed() const;
    
private:
    // 异步读取数据
    void doRead();
    
    // 异步写入数据
    void doWrite();
};
```

### 2.3 TcpServer

#### 2.3.1 功能描述
TCP服务器负责监听和接受TCP连接，管理所有客户端连接，提供TCP协议的通信服务。

#### 2.3.2 实现细节
- 基于Boost::asio的acceptor实现
- 支持设置监听地址和端口
- 提供连接管理功能
- 支持设置消息处理和连接关闭回调

```cpp
class TcpServer {
private:
    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    Connection::MessageHandler message_handler_;
    Connection::CloseHandler close_handler_;
    std::atomic<bool> running_;
    std::unordered_set<Connection::Ptr> connections_;
    std::mutex connections_mutex_;

public:
    TcpServer(boost::asio::io_context& io_context, const std::string& address, uint16_t port);
    ~TcpServer();
    
    // 启动服务器
    void start();
    
    // 停止服务器
    void stop();
    
    // 设置消息处理回调
    void setMessageHandler(Connection::MessageHandler handler);
    
    // 设置连接关闭回调
    void setCloseHandler(Connection::CloseHandler handler);
    
    // 服务器状态检查
    bool isRunning() const;
    
private:
    // 异步接受连接
    void doAccept();
    
    // 处理新连接
    void handleAccept(boost::system::error_code ec, boost::asio::ip::tcp::socket socket);
    
    // 移除连接
    void removeConnection(Connection::Ptr conn);
};
```

### 2.4 WebSocketServer

#### 2.4.1 功能描述
WebSocket服务器负责监听和接受WebSocket连接，管理所有客户端连接，提供WebSocket协议的通信服务。

#### 2.4.2 实现细节
- 基于Boost::asio的acceptor实现
- 支持WebSocket协议的握手和数据传输
- 提供连接管理功能
- 支持设置消息处理和连接关闭回调

```cpp
class WebSocketServer {
private:
    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    Connection::MessageHandler message_handler_;
    Connection::CloseHandler close_handler_;
    std::atomic<bool> running_;
    std::unordered_set<Connection::Ptr> connections_;
    std::mutex connections_mutex_;

public:
    WebSocketServer(boost::asio::io_context& io_context, const std::string& address, uint16_t port);
    ~WebSocketServer();
    
    // 启动服务器
    void start();
    
    // 停止服务器
    void stop();
    
    // 设置消息处理回调
    void setMessageHandler(Connection::MessageHandler handler);
    
    // 设置连接关闭回调
    void setCloseHandler(Connection::CloseHandler handler);
    
    // 服务器状态检查
    bool isRunning() const;
    
private:
    // 异步接受连接
    void doAccept();
    
    // 处理新连接
    void handleAccept(boost::system::error_code ec, boost::asio::ip::tcp::socket socket);
    
    // 移除连接
    void removeConnection(Connection::Ptr conn);
    
    // WebSocket握手处理
    void handleWebSocketHandshake(Connection::Ptr conn, const std::vector<char>& data);
    
    // WebSocket数据帧处理
    void handleWebSocketFrame(Connection::Ptr conn, const std::vector<char>& data);
};
```

## 3. 数据流和交互流程

### 3.1 客户端连接流程

1. 客户端发起TCP/WebSocket连接请求
2. TcpServer/WebSocketServer接受连接请求
3. 创建Connection对象管理新连接
4. 调用Connection::start()开始处理数据传输
5. 触发连接建立事件，通知上层模块

```
客户端 → TcpServer/WebSocketServer → Connection → 上层模块
```

### 3.2 消息接收流程

1. Connection对象接收到网络数据
2. 解析数据帧（如果是WebSocket协议）
3. 调用消息处理回调函数
4. 将数据传递给上层模块处理

```
客户端 → Connection → 消息处理回调 → 上层模块
```

### 3.3 消息发送流程

1. 上层模块调用Connection::send()发送数据
2. Connection对象将数据添加到发送缓冲区
3. 异步写入网络
4. 处理发送完成事件

```
上层模块 → Connection → 网络 → 客户端
```

### 3.4 客户端断开连接流程

1. 检测到客户端断开连接
2. 调用连接关闭回调函数
3. 清理连接资源
4. 通知上层模块

```
客户端断开 → Connection → 关闭处理回调 → 上层模块
```

## 4. 技术特点和优势

### 4.1 高性能
- 基于Boost::asio的事件驱动IO模型
- 支持高并发连接处理
- 低延迟的事件处理机制
- 高效的内存管理

### 4.2 可靠性
- 完善的错误处理机制
- 连接状态监控和自动重连（可选）
- 数据完整性检查
- 优雅的关闭流程

### 4.3 灵活性
- 支持多种协议（TCP、WebSocket）
- 可扩展的事件处理机制
- 灵活的配置选项
- 易于集成到不同的应用架构中

### 4.4 易用性
- 简洁的API设计
- 丰富的文档和示例
- 统一的接口风格
- 易于使用和扩展

## 5. 使用示例

### 5.1 TCP服务器使用示例

```cpp
#include "network/EventLoop.h"
#include "network/TcpServer.h"

int main() {
    // 创建事件循环
    network::EventLoop event_loop;
    
    // 创建TCP服务器
    network::TcpServer tcp_server(event_loop.getIoContext(), "0.0.0.0", 8000);
    
    // 设置消息处理回调
    tcp_server.setMessageHandler([](network::Connection::Ptr conn, const std::vector<char>& data) {
        // 处理接收到的消息
        std::string message(data.begin(), data.end());
        std::cout << "Received: " << message << std::endl;
        
        // 回显消息
        std::string response = "Echo: " + message;
        conn->send(std::vector<char>(response.begin(), response.end()));
    });
    
    // 设置连接关闭回调
    tcp_server.setCloseHandler([](network::Connection::Ptr conn) {
        std::cout << "Connection closed" << std::endl;
    });
    
    // 启动事件循环
    event_loop.start();
    
    // 启动TCP服务器
    tcp_server.start();
    
    std::cout << "TCP Server is running on port 8000" << std::endl;
    
    // 等待用户输入
    std::string input;
    std::cin >> input;
    
    // 停止TCP服务器
    tcp_server.stop();
    
    // 停止事件循环
    event_loop.stop();
    
    return 0;
}
```

### 5.2 WebSocket服务器使用示例

```cpp
#include "network/EventLoop.h"
#include "network/WebSocketServer.h"

int main() {
    // 创建事件循环
    network::EventLoop event_loop;
    
    // 创建WebSocket服务器
    network::WebSocketServer ws_server(event_loop.getIoContext(), "0.0.0.0", 8081);
    
    // 设置消息处理回调
    ws_server.setMessageHandler([](network::Connection::Ptr conn, const std::vector<char>& data) {
        // 处理接收到的消息
        std::string message(data.begin(), data.end());
        std::cout << "Received: " << message << std::endl;
        
        // 回显消息
        std::string response = "Echo: " + message;
        conn->send(std::vector<char>(response.begin(), response.end()));
    });
    
    // 设置连接关闭回调
    ws_server.setCloseHandler([](network::Connection::Ptr conn) {
        std::cout << "Connection closed" << std::endl;
    });
    
    // 启动事件循环
    event_loop.start();
    
    // 启动WebSocket服务器
    ws_server.start();
    
    std::cout << "WebSocket Server is running on port 8081" << std::endl;
    
    // 等待用户输入
    std::string input;
    std::cin >> input;
    
    // 停止WebSocket服务器
    ws_server.stop();
    
    // 停止事件循环
    event_loop.stop();
    
    return 0;
}
```

## 6. 未来扩展计划

### 6.1 功能扩展
- 支持更多网络协议（如UDP、QUIC）
- 添加SSL/TLS加密支持
- 实现流量控制和拥塞控制
- 支持负载均衡功能

### 6.2 性能优化
- 内存池优化
- 零拷贝技术应用
- 更高效的事件处理算法
- 多线程优化

### 6.3 可靠性增强
- 连接池管理
- 自动重连机制
- 故障转移支持
- 监控和统计功能

## 7. 总结

网络层是IMServer的重要组成部分，提供了高性能、可靠的网络通信服务。该模块基于Boost::asio实现，采用事件驱动的IO模型，支持多种协议，能够高效处理大量并发连接。网络层的设计遵循了高内聚、低耦合的原则，易于使用和扩展，为IMServer的稳定运行提供了坚实的基础。