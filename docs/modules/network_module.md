# IMServer 网络模块技术文档

## 1. 模块概述

网络模块是IMServer的核心组件之一，负责处理所有网络通信。它基于Boost::asio实现，提供了高效、可扩展的网络通信能力，支持TCP和WebSocket两种协议。

### 1.1 主要功能

- **事件驱动架构**：基于Boost::asio的事件系统，实现高效的事件处理
- **多协议支持**：同时支持TCP和WebSocket协议
- **多线程并发**：内置线程池，支持高效并发处理
- **连接管理**：自动处理连接的建立、数据传输和关闭
- **异步通信**：全异步API设计，避免阻塞
- **事件系统**：支持事件的注册、注销和触发，实现模块间的松耦合通信

### 1.2 架构设计

网络模块采用分层设计，主要包括以下几个层次：

- **事件系统层**：基于Boost::asio的EventSystem实现事件驱动
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

### 3.1 EventSystem

#### 3.1.1 功能描述

EventSystem是网络模块的核心事件驱动组件，负责管理事件和事件监听器，提供高效的异步事件处理机制。它采用单例模式设计，确保全局只有一个事件系统实例。

主要功能包括：
- 事件的注册、注销和触发
- 事件监听器的添加和移除
- 异步事件处理
- 事件优先级支持
- 批量事件发布
- 同步/异步事件处理

#### 3.1.2 设计理念

EventSystem采用观察者模式设计，将事件的发布者和订阅者解耦，实现模块间的松耦合通信。它基于Boost::asio的io_context实现异步事件处理，确保高效的事件驱动机制。

#### 3.1.3 核心类结构

**Event（事件基类）**

```cpp
class Event {
public:
    virtual ~Event() = default;
    virtual std::type_index getType() const = 0;
    virtual std::string getName() const = 0;
    int priority = 0;
    std::chrono::steady_clock::time_point timestamp;
    Event() : timestamp(std::chrono::steady_clock::now()) {}
};
```

所有事件都必须继承自Event类，实现getType()和getName()方法。事件可以设置优先级，高优先级的事件会被优先处理。

**EventListener（事件监听器）**

```cpp
template<typename T>
class EventListener : public EventListenerBase {
public:
    using CallbackType = std::function<void(const std::shared_ptr<T>&)>;
    explicit EventListener(CallbackType callback) : callback_(std::move(callback)) {}
    void onEvent(const std::shared_ptr<Event>& event) override {
        auto typedEvent = std::dynamic_pointer_cast<T>(event);
        if (typedEvent && callback_) {
            callback_(typedEvent);
        }
    }
private:
    CallbackType callback_;
};
```

事件监听器是一个模板类，支持类型安全的事件处理。监听器通过回调函数接收并处理特定类型的事件。

**EventSystem（事件系统核心类）**

```cpp
class EventSystem {
public:
    static EventSystem& getInstance();
    EventSystem(const EventSystem&) = delete;
    EventSystem& operator=(const EventSystem&) = delete;
    ~EventSystem();
    void start();
    void stop();
    template<typename T>
    size_t subscribe(typename EventListener<T>::CallbackType callback);
    template<typename T>
    bool unsubscribe(size_t listenerId);
    template<typename T>
    void publish(std::shared_ptr<T> event);
    void publishBatch(std::vector<std::shared_ptr<Event>> events);
    template<typename T>
    void dispatch(std::shared_ptr<T> event);
    size_t getPendingEventCount() const;
    void clearQueue();
private:
    EventSystem();
    void processEvents();
    std::unordered_map<std::type_index, std::vector<std::shared_ptr<EventListenerBase>>> listeners_;
    std::priority_queue<EventQueueItem> eventQueue_;
    std::mutex mutex_;           // 保护监听器列表
    std::mutex queueMutex_;      // 保护事件队列
    std::condition_variable cv_; // 条件变量
    std::thread workerThread_;   // 工作线程
    std::atomic<bool> running_;  // 运行状态
    size_t nextListenerId_;      // 监听器ID生成器
    std::atomic<size_t> eventCount_; // 待处理事件计数
};
```

EventSystem是事件系统的核心类，采用单例模式设计。它负责管理事件监听器和事件队列，处理事件的发布和分发。

#### 3.1.4 主要方法详解

**启动和停止事件系统**

```cpp
void start();  // 启动事件系统，开始异步处理事件
void stop();   // 停止事件系统，等待所有事件处理完成
```

**注册和注销事件监听器**

```cpp
template<typename T>
size_t subscribe(typename EventListener<T>::CallbackType callback);  // 注册事件监听器

template<typename T>
bool unsubscribe(size_t listenerId);  // 注销事件监听器
```

**发布事件**

```cpp
template<typename T>
void publish(std::shared_ptr<T> event);  // 异步发布事件

void publishBatch(std::vector<std::shared_ptr<Event>> events);  // 批量异步发布事件

template<typename T>
void dispatch(std::shared_ptr<T> event);  // 同步处理事件（立即执行）
```

**事件队列管理**

```cpp
size_t getPendingEventCount() const;  // 获取当前待处理事件数量
void clearQueue();  // 清空事件队列
```

#### 3.1.5 事件处理流程

1. **事件注册**：模块通过subscribe()方法注册对特定类型事件的监听器
2. **事件发布**：模块通过publish()或publishBatch()方法发布事件
3. **事件入队**：事件被添加到优先级队列中
4. **事件分发**：工作线程从队列中获取事件，并分发给所有注册的监听器
5. **事件处理**：监听器的回调函数被调用，处理事件

#### 3.1.6 使用示例

```cpp
#include <iostream>
#include "network/EventSystem.h"

using namespace im::network;

// 自定义事件类
class UserLoginEvent : public Event {
public:
    UserLoginEvent(std::string username) : username_(std::move(username)) {}
    std::type_index getType() const override {
        return std::type_index(typeid(UserLoginEvent));
    }
    std::string getName() const override {
        return "UserLoginEvent";
    }
    std::string getUsername() const { return username_; }
private:
    std::string username_;
};

int main() {
    // 获取事件系统实例
    auto& eventSystem = EventSystem::getInstance();
    
    // 启动事件系统
    eventSystem.start();
    
    // 注册事件监听器
    auto listenerId = eventSystem.subscribe<UserLoginEvent>(
        [](const std::shared_ptr<UserLoginEvent>& event) {
            std::cout << "User logged in: " << event->getUsername() << std::endl;
        }
    );
    
    // 发布事件
    eventSystem.publish(std::make_shared<UserLoginEvent>("admin"));
    eventSystem.publish(std::make_shared<UserLoginEvent>("user123"));
    
    // 等待事件处理完成
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 注销事件监听器
    eventSystem.unsubscribe<UserLoginEvent>(listenerId);
    
    // 停止事件系统
    eventSystem.stop();
    
    return 0;
}
```

#### 3.1.7 线程安全保证

- **单例实例安全**：通过静态局部变量实现线程安全的单例实例创建
- **事件发布安全**：publish()和dispatch()方法是线程安全的，可以从任何线程调用
- **监听器管理安全**：subscribe()和unsubscribe()方法使用互斥锁保护监听器列表
- **事件队列安全**：使用互斥锁保护事件队列的访问

#### 3.1.8 性能优化

- **事件优先级队列**：使用优先级队列确保高优先级事件被优先处理
- **批量事件发布**：publishBatch()方法支持批量发布事件，减少锁竞争
- **类型安全的事件处理**：使用动态类型转换确保类型安全的事件处理
- **异步事件处理**：基于Boost::asio的io_context实现高效的异步事件处理

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
    EventSystem& event_system_;
    boost::asio::ip::tcp::acceptor acceptor_;
    Connection::MessageHandler message_handler_;
    Connection::CloseHandler close_handler_;
    
    void startAccept();
    void handleAccept(boost::asio::ip::tcp::socket socket, const boost::system::error_code& ec);

public:
    TcpServer(EventSystem& event_system, const std::string& host, uint16_t port);
    
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
- 与TCP服务器共享事件系统

```cpp
class WebSocketServer {
private:
    EventSystem& event_system_;
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
    WebSocketServer(EventSystem& event_system, const std::string& host, uint16_t port);
    
    void start();
    void stop();
};
```

## 4. 线程模型

网络模块采用了高效的线程模型，主要特点包括：

### 4.1 单EventSystem多线程

- 所有网络事件共享同一个EventSystem
- EventSystem内部管理一个工作线程
- 工作线程负责事件的分发和处理
- 事件监听器的回调函数在工作线程中执行

### 4.2 线程安全考虑

- 所有公共API都是线程安全的
- 使用互斥锁保护共享数据
- 避免在事件处理回调中进行长时间阻塞操作
- 事件的发布和处理是异步的，不会阻塞调用线程

## 5. 性能优化

### 5.1 事件处理优化

- **事件优先级**：支持事件优先级，确保重要事件被优先处理
- **批量事件发布**：减少锁竞争，提高批量事件处理效率
- **异步事件处理**：基于Boost::asio的io_context实现高效的异步事件处理

### 5.2 内存管理

- 使用固定大小的缓冲区减少内存分配
- 采用对象池技术复用Connection对象
- 使用智能指针自动管理内存，避免内存泄漏

### 5.3 I/O优化

- 全异步I/O操作，避免阻塞
- 合理设置套接字选项（如TCP_NODELAY）

### 5.4 连接管理

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
#include "network/EventSystem.h"
#include "network/tcp_server.h"

using namespace im::network;
using namespace std;

int main() {
    // 获取事件系统实例
    auto& event_system = EventSystem::getInstance();
    
    // 创建TCP服务器
    TcpServer tcp_server(event_system, "0.0.0.0", 8080);
    
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
    
    // 启动事件系统和服务器
    event_system.start();
    tcp_server.start();
    
    // 等待用户输入
    getchar();
    
    // 停止服务器和事件系统
    tcp_server.stop();
    event_system.stop();
    
    return 0;
}
```

### 7.2 WebSocket服务器示例

```cpp
#include "network/EventSystem.h"
#include "network/websocket_server.h"

using namespace im::network;
using namespace std;

int main() {
    // 获取事件系统实例
    auto& event_system = EventSystem::getInstance();
    
    // 创建WebSocket服务器
    WebSocketServer ws_server(event_system, "0.0.0.0", 8081);
    
    // 启动事件系统和服务器
    event_system.start();
    ws_server.start();
    
    // 等待用户输入
    getchar();
    
    // 停止服务器和事件系统
    ws_server.stop();
    event_system.stop();
    
    return 0;
}
```

### 7.3 事件系统使用示例

```cpp
#include <iostream>
#include "network/EventSystem.h"

using namespace im::network;

// 自定义事件类
class UserLoginEvent : public Event {
public:
    UserLoginEvent(std::string username) : username_(std::move(username)) {}
    std::type_index getType() const override { return std::type_index(typeid(UserLoginEvent)); }
    std::string getName() const override { return "UserLoginEvent"; }
    std::string getUsername() const { return username_; }
private:
    std::string username_;
};

class MessageReceivedEvent : public Event {
public:
    MessageReceivedEvent(std::string sender, std::string content) 
        : sender_(std::move(sender)), content_(std::move(content)) {}
    std::type_index getType() const override { return std::type_index(typeid(MessageReceivedEvent)); }
    std::string getName() const override { return "MessageReceivedEvent"; }
    std::string getSender() const { return sender_; }
    std::string getContent() const { return content_; }
private:
    std::string sender_;
    std::string content_;
};

int main() {
    // 获取事件系统实例
    auto& event_system = EventSystem::getInstance();
    
    // 注册事件监听器
    auto loginListenerId = event_system.subscribe<UserLoginEvent>(
        [](const std::shared_ptr<UserLoginEvent>& event) {
            std::cout << "[Login] User: " << event->getUsername() << std::endl;
        }
    );
    
    auto messageListenerId = event_system.subscribe<MessageReceivedEvent>(
        [](const std::shared_ptr<MessageReceivedEvent>& event) {
            std::cout << "[Message] From: " << event->getSender() 
                      << ", Content: " << event->getContent() << std::endl;
        }
    );
    
    // 启动事件系统
    event_system.start();
    
    // 发布事件
    event_system.publish(std::make_shared<UserLoginEvent>("admin"));
    event_system.publish(std::make_shared<MessageReceivedEvent>("admin", "Hello, world!"));
    event_system.publish(std::make_shared<UserLoginEvent>("user123"));
    event_system.publish(std::make_shared<MessageReceivedEvent>("user123", "Hi there!"));
    
    // 批量发布事件
    std::vector<std::shared_ptr<Event>> events;
    events.push_back(std::make_shared<MessageReceivedEvent>("admin", "How are you?"));
    events.push_back(std::make_shared<MessageReceivedEvent>("user123", "I'm fine, thanks!"));
    event_system.publishBatch(events);
    
    // 等待事件处理完成
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 注销事件监听器
    event_system.unsubscribe<UserLoginEvent>(loginListenerId);
    event_system.unsubscribe<MessageReceivedEvent>(messageListenerId);
    
    // 停止事件系统
    event_system.stop();
    
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

- **更高效的事件循环**：探索使用io_uring等新的I/O技术
- **连接池**：实现客户端连接池，提高连接复用率
- **事件批处理优化**：进一步优化批量事件处理性能

### 8.3 可观测性

- **性能监控**：添加性能指标收集和监控
- **日志增强**：更详细的日志记录，便于问题定位
- **追踪支持**：添加分布式追踪功能，支持跨服务调用追踪

### 8.4 事件系统增强

- **事件过滤**：支持基于条件的事件过滤
- **事件溯源**：添加事件溯源功能，支持事件历史查询
- **事务支持**：添加事务支持，确保事件处理的原子性

## 9. 总结

网络模块是IMServer的核心组件之一，基于Boost::asio实现了高效、可扩展的网络通信能力。它采用了事件驱动、异步I/O和多线程并发的设计，支持TCP和WebSocket两种协议，具有良好的性能和可靠性。

EventSystem作为网络模块的核心组件，实现了高效的事件驱动机制，采用观察者模式将事件的发布者和订阅者解耦，实现模块间的松耦合通信。它支持事件优先级、批量事件发布、同步/异步事件处理等功能，为网络模块提供了灵活、高效的事件处理能力。

通过合理的线程模型和性能优化，网络模块能够处理大量并发连接，满足IM系统的高并发需求。未来，网络模块将继续扩展功能和优化性能，以支持更多的应用场景和更高的性能要求。