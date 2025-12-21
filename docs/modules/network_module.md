# IMServer 网络模块技术文档

## 1. 模块概述

网络模块是IMServer的核心组件之一，负责处理所有网络通信。它基于Boost::asio实现，提供了高效、可扩展的网络通信能力，支持TCP和WebSocket两种协议。

### 1.1 主要功能

- **高性能事件驱动**：基于Boost::asio的事件循环机制
- **多协议支持**：同时支持TCP和WebSocket协议
- **多线程并发**：内置线程池，支持高效并发处理
- **连接管理**：自动处理连接的建立、数据传输和关闭
- **异步通信**：全异步API设计，避免阻塞

### 1.2 架构设计

网络模块采用分层设计，主要包括以下几个层次：

- **事件驱动层**：基于Boost::asio的io_context
- **协议层**：TCP和WebSocket协议实现
- **连接管理层**：处理客户端连接的生命周期
- **数据处理层**：负责数据的编解码和分发

## 2. Boost::asio选择理由

选择Boost::asio作为网络库框架的主要原因包括：

- **高性能**：基于事件驱动和异步I/O模型，效率高
- **跨平台**：支持Windows、Linux、macOS等多种操作系统
- **稳定成熟**：经过长期发展和广泛应用的成熟库
- **丰富功能**：支持TCP、UDP、WebSocket等多种网络协议
- **C++标准兼容**：与C++17标准良好集成
- **活跃社区**：有大量的文档和示例可供参考

## 3. 核心组件设计

### 3.1 EventLoop

#### 3.1.1 功能描述
事件循环是网络模块的核心，负责处理所有网络事件（连接、数据可读、数据可写等）。它提供了一个高效的异步事件处理机制，并支持多线程并发处理。

#### 3.1.2 实现细节
- 基于`boost::asio::io_context`构建
- **完整的线程池支持**：可配置线程数量，默认1个线程
- 提供定时器功能
- 事件回调机制
- 线程安全的任务提交接口

```cpp
class EventLoop {
private:
    boost::asio::io_context io_context_;
    std::unique_ptr<boost::asio::io_context::work> work_;
    std::vector<std::thread> threads_;

public:
    /**
     * @brief 构造函数
     * @param thread_pool_size 线程池大小，默认为1
     */
    explicit EventLoop(size_t thread_pool_size = 1);
    
    /**
     * @brief 析构函数
     */
    ~EventLoop();
    
    /**
     * @brief 启动事件循环
     */
    void start();
    
    /**
     * @brief 停止事件循环
     */
    void stop();
    
    /**
     * @brief 将任务提交到事件循环（异步执行，不阻塞）
     * @tparam Func 任务函数类型
     * @param func 任务函数
     */
    template<typename Func>
    void post(Func&& func) {
        io_context_.post(std::forward<Func>(func));
    }
    
    /**
     * @brief 将任务提交到事件循环（如果在事件循环线程中则直接执行，否则异步执行）
     * @tparam Func 任务函数类型
     * @param func 任务函数
     */
    template<typename Func>
    void dispatch(Func&& func) {
        io_context_.dispatch(std::forward<Func>(func));
    }
    
    /**
     * @brief 获取io_context
     * @return boost::asio::io_context& io_context引用
     */
    boost::asio::io_context& getIOContext() {
        return io_context_;
    }
    
    /**
     * @brief 获取线程池大小
     * @return 线程池大小
     */
    size_t getThreadPoolSize() const {
        return threads_.size();
    }
};
```

#### 3.1.3 线程池实现

EventLoop的线程池实现基于`boost::asio::io_context`，主要特点包括：

1. **动态线程数量**：支持在构造时指定线程池大小
2. **线程安全**：所有公共接口都是线程安全的
3. **优雅启动和停止**：提供`start()`和`stop()`方法进行生命周期管理
4. **异常处理**：线程内部捕获并处理异常，避免程序崩溃
5. **灵活的任务提交**：支持`post()`（异步执行）和`dispatch()`（同步/异步执行）两种方式

#### 3.1.4 使用示例

```cpp
#include <iostream>
#include <chrono>
#include "network/event_loop.h"

using namespace im;
using namespace std;
using namespace chrono;

int main() {
    // 创建一个包含4个线程的EventLoop
    EventLoop event_loop(4);
    
    // 启动事件循环
    event_loop.start();
    
    // 提交一些任务到事件循环
    for (int i = 0; i < 10; ++i) {
        event_loop.post([i]() {
            cout << "Task " << i << " is running on thread " 
                 << this_thread::get_id() << endl;
            
            // 模拟任务执行时间
            this_thread::sleep_for(100ms);
        });
    }
    
    // 等待所有任务完成
    this_thread::sleep_for(2s);
    
    // 停止事件循环
    event_loop.stop();
    
    cout << "All tasks completed, event loop stopped." << endl;
    
    return 0;
}
```

#### 3.1.5 性能优化

- **线程数量选择**：建议设置为CPU核心数或核心数+1
- **任务粒度**：尽量将大任务拆分为小任务，提高并发效率
- **避免阻塞操作**：在事件循环中避免长时间阻塞的操作，如需阻塞应使用独立线程
- **定时器优化**：对于大量定时器，考虑使用分层时间轮算法

#### 3.1.6 线程模型

EventLoop采用"一个io_context多个线程"的模型：

- 所有线程共享同一个`io_context`
- `io_context`负责事件的分发和任务的执行
- 线程池中的线程从`io_context`中获取任务并执行
- 任务执行顺序不保证，但同一连接的事件会在同一线程中处理，避免锁竞争

### 3.2 Connection

#### 3.2.1 功能描述
管理单个客户端连接，处理数据的发送和接收。

#### 3.2.2 实现细节
- 基于`boost::asio::ip::tcp::socket`
- 支持异步读写操作
- 自动处理连接生命周期
- 缓冲区管理

```cpp
class Connection : public std::enable_shared_from_this<Connection> {
private:
    boost::asio::ip::tcp::socket socket_;
    std::array<char, 8192> read_buffer_;
    std::queue<std::vector<char>> write_queue_;
    bool writing_;
    
    void startRead();
    void handleRead(const boost::system::error_code& ec, size_t bytes_transferred);
    void startWrite();
    void handleWrite(const boost::system::error_code& ec, size_t bytes_transferred);

public:
    using Ptr = std::shared_ptr<Connection>;
    using MessageHandler = std::function<void(Ptr, const std::vector<char>&)>;
    using CloseHandler = std::function<void(Ptr)>;
    
    Connection(boost::asio::ip::tcp::socket socket, MessageHandler message_handler, CloseHandler close_handler);
    
    void start();
    void send(const std::vector<char>& data);
    void close();
    
    boost::asio::ip::tcp::endpoint getRemoteEndpoint() const;
};
```

### 3.3 TcpServer

#### 3.3.1 功能描述
TCP服务器，负责监听TCP端口，接受客户端连接。

#### 3.3.2 实现细节
- 基于`boost::asio::ip::tcp::acceptor`
- 支持异步接受连接
- 自动管理连接的生命周期
- 提供连接事件回调

```cpp
class TcpServer {
private:
    EventLoop& event_loop_;
    boost::asio::ip::tcp::acceptor acceptor_;
    Connection::MessageHandler message_handler_;
    Connection::CloseHandler close_handler_;
    
    void startAccept();
    void handleAccept(boost::asio::ip::tcp::socket socket, const boost::system::error_code& ec);

public:
    TcpServer(EventLoop& event_loop, const std::string& host, uint16_t port);
    
    void start();
    void stop();
    
    void setMessageHandler(Connection::MessageHandler handler) {
        message_handler_ = std::move(handler);
    }
    
    void setCloseHandler(Connection::CloseHandler handler) {
        close_handler_ = std::move(handler);
    }
};
```

### 3.4 WebSocketServer

#### 3.4.1 功能描述
WebSocket服务器，负责监听WebSocket连接，支持浏览器客户端。

#### 3.4.2 实现细节
- 基于`boost::beast::websocket::stream`
- 支持异步WebSocket握手和数据传输
- 兼容WebSocket协议规范
- 与TCP服务器共享事件循环

```cpp
class WebSocketServer {
private:
    EventLoop& event_loop_;
    boost::asio::ip::tcp::acceptor acceptor_;
    
    void startAccept();
    void handleAccept(boost::asio::ip::tcp::socket socket, const boost::system::error_code& ec);
    
    class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
    private:
        boost::beast::websocket::stream<boost::asio::ip::tcp::socket> ws_;
        boost::beast::flat_buffer buffer_;
        
        void onAccept(const boost::system::error_code& ec);
        void doRead();
        void onRead(const boost::system::error_code& ec, size_t bytes_transferred);
        void onWrite(const boost::system::error_code& ec, size_t bytes_transferred);
        
    public:
        explicit WebSocketSession(boost::asio::ip::tcp::socket socket);
        void start();
        void send(const std::string& message);
    };

public:
    WebSocketServer(EventLoop& event_loop, const std::string& host, uint16_t port);
    
    void start();
    void stop();
};
```

## 4. 线程模型

网络模块采用了高效的线程模型，主要特点包括：

### 4.1 单EventLoop多线程

- 所有网络事件共享同一个EventLoop
- EventLoop内部管理一个线程池
- 线程池中的线程并发处理事件
- 同一连接的事件在同一线程中处理，避免锁竞争

### 4.2 线程安全考虑

- 所有公共API都是线程安全的
- 使用`post()`和`dispatch()`方法确保线程安全
- 避免在事件处理回调中进行长时间阻塞操作

## 5. 性能优化

### 5.1 内存管理

- 使用固定大小的缓冲区减少内存分配
- 采用对象池技术复用Connection对象
- 使用智能指针自动管理内存，避免内存泄漏

### 5.2 I/O优化

- 全异步I/O操作，避免阻塞
- 使用`io_context::run()`的多线程版本，提高并发能力
- 合理设置套接字选项（如TCP_NODELAY）

### 5.3 连接管理

- 自动超时断开空闲连接
- 限制最大连接数，防止资源耗尽
- 连接状态监控，及时释放资源

## 6. 错误处理

### 6.1 异常处理

- 线程内部捕获并处理所有异常
- 错误日志记录，便于问题定位
- 优雅关闭出错的连接

### 6.2 错误恢复

- 网络异常自动重试机制
- 服务器重启后自动恢复监听
- 连接断开后客户端可以自动重连

## 7. 集成使用示例

### 7.1 TCP服务器示例

```cpp
#include "network/event_loop.h"
#include "network/tcp_server.h"

using namespace im;
using namespace std;

int main() {
    // 创建事件循环
    EventLoop event_loop(4);
    
    // 创建TCP服务器
    TcpServer tcp_server(event_loop, "0.0.0.0", 8080);
    
    // 设置消息处理函数
    tcp_server.setMessageHandler([](Connection::Ptr conn, const vector<char>& data) {
        cout << "Received message: " << string(data.begin(), data.end()) << endl;
        
        // 回显消息
        conn->send(data);
    });
    
    // 设置连接关闭处理函数
    tcp_server.setCloseHandler([](Connection::Ptr conn) {
        cout << "Connection closed: " << conn->getRemoteEndpoint().address().to_string() << endl;
    });
    
    // 启动服务器和事件循环
    tcp_server.start();
    event_loop.start();
    
    // 等待用户输入
    getchar();
    
    // 停止服务器和事件循环
    tcp_server.stop();
    event_loop.stop();
    
    return 0;
}
```

### 7.2 WebSocket服务器示例

```cpp
#include "network/event_loop.h"
#include "network/websocket_server.h"

using namespace im;
using namespace std;

int main() {
    // 创建事件循环
    EventLoop event_loop(4);
    
    // 创建WebSocket服务器
    WebSocketServer ws_server(event_loop, "0.0.0.0", 8081);
    
    // 启动服务器和事件循环
    ws_server.start();
    event_loop.start();
    
    // 等待用户输入
    getchar();
    
    // 停止服务器和事件循环
    ws_server.stop();
    event_loop.stop();
    
    return 0;
}
```

## 8. 未来扩展计划

### 8.1 功能扩展

- **SSL/TLS支持**：为TCP和WebSocket添加加密支持
- **UDP协议**：支持UDP协议，用于实时音视频通信
- **HTTP/HTTPS支持**：添加HTTP服务器功能，支持RESTful API
- **负载均衡**：支持多服务器部署和负载均衡

### 8.2 性能优化

- **零拷贝技术**：减少数据拷贝，提高性能
- **更高效的事件循环**：探索使用io_uring等新的I/O技术
- **连接池**：实现客户端连接池，提高连接复用率

### 8.3 可观测性

- **性能监控**：添加性能指标收集和监控
- **日志增强**：更详细的日志记录，便于问题定位
- **追踪支持**：添加分布式追踪功能，支持跨服务调用追踪

## 9. 总结

网络模块是IMServer的核心组件之一，基于Boost::asio实现了高效、可扩展的网络通信能力。它采用了事件驱动、异步I/O和多线程并发的设计，支持TCP和WebSocket两种协议，具有良好的性能和可靠性。通过合理的线程模型和性能优化，网络模块能够处理大量并发连接，满足IM系统的高并发需求。

未来，网络模块将继续扩展功能和优化性能，以支持更多的应用场景和更高的性能要求。