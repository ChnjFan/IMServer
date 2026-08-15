# 基于 Boost.Beast 构建 C++ HTTP 服务器 —— IMServer GateServer 实战

> 从零构建一个生产级 C++ HTTP 网关：多线程 io_context 池、异步连接管理、路径路由分发、JSON 协议转换、gRPC 后端集成。

## 第一章：项目背景与总体架构

### 1.1 IMServer 是什么

IMServer 是一个多服务即时消息后端，由四个独立进程组成：

| 服务 | 职责 |
|------|------|
| **GateServer** | HTTP 网关，面向客户端提供 REST API（注册、登录、验证码等） |
| **ChatServer** | TCP 长连接服务，核心 IM 消息路由 |
| **StatusServer** | gRPC 服务，用户在线状态与 ChatServer 负载均衡 |
| **VerifyServer** | Node.js 服务，邮件验证码生成与发送 |

本文聚焦于 **GateServer** —— 它是客户端 HTTP 请求的唯一入口，负责接收 REST API 调用、校验参数、转发到后端 gRPC 服务、返回 JSON 结果。

### 1.2 GateServer 在架构中的位置

```
客户端 (HTTP)
    │
    ▼
┌─────────────┐
│  GateServer  │  ◄── 本文主角
│  (Boost.Beast)│
└──────┬───────┘
       │ gRPC
       ▼
┌──────────────┐  ┌──────────────┐  ┌──────────────┐
│ StatusServer  │  │ VerifyServer  │  │  ChatServer   │
│ (负载均衡)     │  │ (验证码)       │  │ (TCP 消息)    │
└──────────────┘  └──────────────┘  └──────────────┘
```

### 1.3 为什么选 Boost.Beast

C++ HTTP 框架不少，Crow、Drogon、oatpp、cpp-httplib 都是热门选择。GateServer 选择 Boost.Beast 的原因：

- **与现有技术栈统一**：项目 ChatServer 已经大量使用 Boost.Asio 处理 TCP，Beast 同属 Boost 体系，io_context、timer、strand 等组件无缝复用
- **粒度控制**：Beast 不强制路由/中间件模式，可以按自己的需求设计分发逻辑
- **协议理解更深入**：Beast 要求你手动管理 request/response 生命周期，迫使你真正理解 HTTP 协议细节，而非被框架的黑盒掩盖
- **无外部依赖引入**：项目已经依赖 Boost，不需要再拉一套新框架

代价是你需要自己处理连接管理、超时、路由分发等「脏活」—— 但这也正是实战教学的价值所在。

---

## 第二章：服务启动与 io_context 池

### 2.1 入口：main.cpp

GateServer 的启动流程异常简洁：

```cpp
// main.cpp:7-31
int main() {
    auto port = std::stoi(ConfigMgr::getInstance()["GateServer"]["Port"]);

    try {
        net::io_context io_context{1};
        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        auto pool = AsioIOServicePool::getInstance();

        signals.async_wait([&io_context, &pool](auto error, int signal_number) {
            if (error) return;
            io_context.stop();
            pool->stop();
        });

        std::make_shared<GateServer>(io_context, port)->start();
        std::cout << "GateServer started on port: " << port << std::endl;
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
```

**关键设计点**：

1. **两个 io_context 分工**：`io_context{1}` 单线程运行 acceptor 监听；`AsioIOServicePool` 内部的多线程 io_context 池处理连接 IO。accept 和处理解耦，避免新连接到来时被数据处理阻塞。

2. **信号监听**：`signal_set` 注册 SIGINT/SIGTERM，触发时同时停止 acceptor 的 io_context 和线程池，实现优雅退出。

3. **配置读取**：`ConfigMgr` 是一个线程安全的单例（Meyer's Singleton），解析 `config.ini` 的 INI 格式配置文件。

### 2.2 io_context 线程池

`AsioIOServicePool` 是整个 HTTP 服务器的并发核心：

```cpp
// AsioIOServicePool.cpp:40-52
AsioIOServicePool::AsioIOServicePool(std::size_t size)
    : ioServices_(size), works_(size), nextIndex_(0) {
    for (std::size_t i = 0; i < size; i++) {
        works_[i] = std::make_unique<Work>(ioServices_[i].get_executor());
    }
    for (std::size_t i = 0; i < size; i++) {
        threads_.emplace_back([this, i]() {
            ioServices_[i].run();
        });
    }
}
```

线程池大小默认为 `hardware_concurrency() - 1`（为主线程留一个核），每个 io_context 配一个独立的 `executor_work_guard`。**work_guard 的作用**是防止 `io_context::run()` 在没有待处理事件时直接返回 —— 如果所有 io_context 瞬间无事可做，线程池就全部退出了。

连接到来时，acceptor 通过轮询策略将新连接分发到某个 io_context：

```cpp
// AsioIOServicePool.cpp:14-19
AsioIOServicePool::IOService& AsioIOServicePool::getIOService() {
    auto& service = ioServices_[nextIndex_++];
    if (nextIndex_ == ioServices_.size()) {
        nextIndex_ = 0;
    }
    return service;
}
```

> **注意**：这里的轮询是无锁的（单生产者），因为只有 acceptor 线程会调用 `getIOService()`。每个 io_context 独立运行在自己的线程上，内部的事件循环天然线程安全。

### 2.3 整体并发模型

```
                    ┌─ io_context[0] (线程0) ── HttpConnection A
                    ├─ io_context[1] (线程1) ── HttpConnection B
io_context (主线程) │  ...                        ...
   Acceptor ──────► ├─ io_context[n-1] (线程n-1) ── HttpConnection X
                    └─ getIOService() 轮询分发
```

每个连接的生命周期（read → handle → write）都在同一个 io_context 上完成，天然避免了跨线程数据竞争。

---

## 第三章：连接接受与生命周期管理

### 3.1 GateServer：Acceptor 封装

GateServer 类本身非常薄，核心就是一个 `tcp::acceptor`：

```cpp
// GateServer.h
class GateServer : public std::enable_shared_from_this<GateServer> {
public:
    GateServer(net::io_context& ioContext, const unsigned short& port);
    void start();
private:
    tcp::acceptor acceptor_;
    net::io_context& ioContext_;
};
```

构造时直接绑定端口：

```cpp
GateServer::GateServer(net::io_context &ioContext, const unsigned short &port)
    : acceptor_(ioContext, tcp::endpoint(tcp::v4(), port))
    , ioContext_(ioContext) {
}
```

### 3.2 async_accept 递归监听

`start()` 是 GateServer 的核心异步操作：

```cpp
// GateServer.cpp:19-39
void GateServer::start() {
    auto self = shared_from_this();
    auto& io_context = AsioIOServicePool::getInstance()->getIOService();
    auto connection = std::make_shared<HttpConnection>(io_context);

    acceptor_.async_accept(connection->getSocket(), [self, connection](auto ec) {
        try {
            if (ec) {
                self->start();  // 出错也要继续监听
                return;
            }
            connection->start();  // 开始处理这个连接
            self->start();        // 继续接受下一个连接
        } catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
        }
    });
}
```

**这个模式有几个值得注意的细节**：

1. **`shared_from_this()` 保活**：lambda 捕获了 `self`（GateServer 的 shared_ptr），确保在异步等待期间 GateServer 不会被析构。这是异步编程中对象生命周期管理的标准做法。

2. **连接在 accept 前就创建**：`HttpConnection` 在 `async_accept` 调用前就构造好了，它的 socket 由 acceptor 直接接入。这避免了新连接到来时还需要先创建对象再转移 socket 的额外开销。

3. **`start()` 递归调用**：accept 完成后立即再次调用 `start()` 进入下一轮异步等待。由于是异步操作，这不是真正的递归栈，而是事件驱动的链式注册。

4. **跨 io_context 分发**：acceptor 在主 io_context 上运行，但 `connection->start()` 中的 `async_read` 会在线程池的 io_context 上执行（因为 HttpConnection 是用线程池的 io_context 构造的）。这是 Boost.Asio 的隐式 `io_context::post` 行为。

### 3.3 连接生命周期全图

```
Client                    GateServer (主io_context)           HttpConnection (线程池io_context)
  │                              │                                    │
  │──── TCP SYN ───────────────►│                                    │
  │                              │─ async_accept 完成 ───────────────►│
  │                              │                                    │
  │                              │   connection->start()              │
  │                              │                                    │
  │──── HTTP Request ──────────►│──── 转发到线程池 ──────────────────►│
  │                              │                          http::async_read
  │                              │                                    │
  │                              │                          handleRequest()
  │                              │                                    │
  │                              │                          writeResponse()
  │                              │                                    │
  │◄──── HTTP Response ──────────│◄─── 转发回来 ──────────────────────│
  │                              │                                    │
  │                              │ next async_accept...               │
```

---

## 第四章：HTTP 请求处理流程

### 4.1 HttpConnection 数据结构

每个连接对应一个 `HttpConnection` 对象，它封装了 HTTP 通信所需的一切状态：

```cpp
// HttpConnection.h:15-36
class HttpConnection : public std::enable_shared_from_this<HttpConnection> {
public:
    explicit HttpConnection(boost::asio::io_context& io_context);
    void start();
    tcp::socket& getSocket() { return socket_; }

private:
    void checkDeadline();
    void writeResponse();
    void handleRequest();

    tcp::socket socket_;
    beast::flat_buffer buffer_{8192};          // 8KB 接收缓冲
    http::request<http::dynamic_body> request_;
    http::response<http::dynamic_body> response_;
    net::steady_timer deadline_{               // 60秒超时定时器
        socket_.get_executor(),
        std::chrono::seconds(60)
    };
    UrlParser urlParser_;
};
```

**各成员的作用**：

| 成员 | 作用 |
|------|------|
| `socket_` | TCP 连接端点 |
| `buffer_` | Beast 解析 HTTP 所需的扁平缓冲区 |
| `request_` / `response_` | HTTP 报文对象，`dynamic_body` 支持任意大小 body |
| `deadline_` | 60 秒无操作自动关闭连接 |
| `urlParser_` | GET 请求的 URL 路径和参数解析 |

### 4.2 异步读取请求

`start()` 方法的职责是发起一次异步 HTTP 读取：

```cpp
// HttpConnection.cpp:15-33
void HttpConnection::start() {
    auto self = shared_from_this();
    http::async_read(socket_, buffer_, request_,
        [self](const boost::system::error_code& ec, size_t bytes_transfer) {
            try {
                if (ec) {
                    std::cout << "http read error: " << ec.what() << std::endl;
                    return;
                }
                boost::ignore_unused(bytes_transfer);
                self->handleRequest();   // 解析完成，处理业务
                self->checkDeadline();   // 重置超时定时器
            } catch (std::exception& e) {
                std::cout << e.what() << std::endl;
            }
    });
}
```

> **与 ChatServer 的对比**：ChatServer 的 TCP 使用自定义二进制协议（4 字节 header + body），需要手动处理粘包/拆包（`MsgNode` 队列）。而 `http::async_read` 是 Beast 提供的高级 API，它能根据 Content-Length 或 Transfer-Encoding 自动读取完整的 HTTP 请求 —— 这是使用 HTTP 层协议最大的便利。

### 4.3 请求分发

`handleRequest()` 根据 HTTP 方法分发到 LogicSystem：

```cpp
// HttpConnection.cpp:64-94
void HttpConnection::handleRequest() {
    response_.version(request_.version());
    response_.keep_alive(false);  // 每个请求关闭连接

    if (request_.method() == http::verb::get) {
        urlParser_.parse(request_.target());
        if (!LogicSystem::getInstance()->handleGet(urlParser_.getPath(), shared_from_this())) {
            response_.result(http::status::not_found);
            response_.set(http::field::content_type, "text/plain");
            beast::ostream(response_.body()) << "url not found\r\n";
            writeResponse();
            return;
        }
        response_.result(http::status::ok);
        response_.set(http::field::server, "GateServer");
        writeResponse();
    }
    else if (request_.method() == http::verb::post) {
        if (!LogicSystem::getInstance()->handlePost(request_.target(), shared_from_this())) {
            response_.result(http::status::not_found);
            // ... 同 GET 的错误处理
        }
        response_.result(http::status::ok);
        response_.set(http::field::server, "GateServer");
        writeResponse();
    }
}
```

可以看到 GET 和 POST 的分发策略略有不同：
- **GET**：先解析 URL（提取路径 + query 参数），再按路径分发
- **POST**：直接用完整 target 路径分发（POST 参数在 body 中）

### 4.4 写入响应与超时控制

响应写入后执行 half-close（只关发送端）：

```cpp
// HttpConnection.cpp:45-62
void HttpConnection::writeResponse() {
    auto self = shared_from_this();
    response_.content_length(response_.body().size());
    http::async_write(socket_, response_,
        [self](const boost::system::error_code& ec, size_t bytes_transfer) {
            if (ec) {
                std::cout << "Socket is shutdown: " << ec.what() << std::endl;
            } else {
                self->socket_.shutdown(tcp::socket::shutdown_send);
            }
            self->deadline_.cancel();  // 关闭 socket 时取消定时器
    });
}

void HttpConnection::checkDeadline() {
    auto self = shared_from_this();
    deadline_.async_wait([self](const boost::system::error_code& ec) {
        if (!ec) {
            self->socket_.close();  // 超时直接关闭
        }
    });
}
```

**超时机制的设计意图**：60 秒内如果没有完成请求-响应循环，定时器触发关闭 socket。但有一个已知问题：

> ⚠️ **TODO**（代码中已有注释）：服务端主动关闭会产生大量 TIME_WAIT 状态的连接，占用文件描述符。在高并发场景下这会成为瓶颈。改进方向见第七章。

---

## 第五章：路由分发与业务处理

### 5.1 LogicSystem：路径-回调映射

`LogicSystem` 是一个 CRTP 单例，内部维护两张 handler 映射表：

```cpp
// LogicSystem.h:15-32
class LogicSystem : public Singleton<LogicSystem> {
public:
    bool handleGet(const std::string& path, const std::shared_ptr<HttpConnection>& connection);
    void registerGet(const std::string& path, const HttpRequestCallback& handler);
    bool handlePost(const std::string& path, const std::shared_ptr<HttpConnection>& connection);
    void registerPost(const std::string& path, const HttpRequestCallback& handler);

private:
    std::unordered_map<std::string, HttpRequestCallback> postHandlers_;
    std::unordered_map<std::string, HttpRequestCallback> getHandlers_;
};
```

handler 签名为 `std::function<void(shared_ptr<HttpConnection>)>`，接收一个 `shared_ptr<HttpConnection>` 参数 —— 通过它可以直接操作 `connection->response_` 写回响应。**LogicSystem 不需要管理连接的生命周期**，因为它只持有弱引用语义的回调参数。

### 5.2 Handler 注册（构造时完成）

所有业务 handler 在 `LogicSystem` 构造函数中注册：

```cpp
// LogicSystem.cpp:49
LogicSystem::LogicSystem() {
    registerGet("/get_test", [](std::shared_ptr<HttpConnection> connection) {
        beast::ostream(connection->response_.body()) << "receive get_test request.\r\n";
        UrlParams urlParams = connection->urlParser_.getParams();
        for (auto& [param, value] : urlParams) {
            beast::ostream(connection->response_.body()) << "Param " << param << "=" << value << "\r\n";
        }
    });

    registerPost("/get_verify_code", [](std::shared_ptr<HttpConnection> connection) {
        auto bodyString = boost::beast::buffers_to_string(connection->request_.body().data());
        // ... JSON 解析，调用 gRPC ...
    });
    // user_register、reset_passwd、user_login 等 handler 同理...
}
```

> **设计考量**：这种「构造时注册全部路由」的方式适合路由表固定的场景。如果需要动态注册路由（如运行时加载插件），可以暴露 `registerGet/Post` 接口给外部调用。

### 5.3 GET 请求示例：/get_test

最简单的 handler，演示 URL 参数解析：

```
GET /get_test?name=alice&age=25 HTTP/1.1
```

会被 `UrlParser` 拆解为：
- path = `/get_test`
- params = `{name: "alice", age: "25"}`

响应：
```
receive get_test request.
Param name=alice
Param age=25
```

### 5.4 POST 请求示例：获取验证码

```cpp
// LogicSystem.cpp:59-94
registerPost("/get_verify_code", [](std::shared_ptr<HttpConnection> connection) {
    auto bodyString = boost::beast::buffers_to_string(connection->request_.body().data());
    connection->response_.set(http::field::content_type, "application/json");

    Json::Value root, srcRoot;
    if (Json::Reader reader; !reader.parse(bodyString, srcRoot)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        boost::beast::ostream(connection->response_.body()) << root.toStyledString();
        return;
    }

    auto email = srcRoot["email"].asString();
    GetVerifyRsp response = VerifyGrpcClient::getInstance()->GetVerifyCode(email);

    root["error"] = response.error();
    root["email"] = email;
    boost::beast::ostream(connection->response_.body()) << root.toStyledString();
});
```

**流程**：HTTP body → 字符串 → JSON 解析 → 提取 email → 调用 gRPC → 构造 JSON 响应。可以看到 handler 内部是**同步阻塞**的 —— gRPC 调用期间 io_context 线程被占用（第七章会讨论改进方案）。

### 5.5 Defer 模式：简化多分支返回

`/user_login` handler 有多个错误分支，每个分支都需要写 JSON 响应。为了不重复调用 `writeResponse()`，代码使用了 `Defer`（RAII）：

```cpp
// LogicSystem.cpp:234-298（简化）
registerPost("/user_login", [](std::shared_ptr<HttpConnection> connection) {
    // ...
    Defer defer([&root, &connection] {  // 析构时自动执行
        const std::string jsonStr = root.toStyledString();
        boost::beast::ostream(connection->response_.body()) << jsonStr;
    });

    if (!reader.parse(bodyString, srcRoot)) {
        root["error"] = ErrorCodes::ERROR_REQUEST_JSON;
        return;  // defer 析构时自动写响应
    }
    if (!MysqlMgr::getInstance()->checkPasswd(email, passwd, userInfo)) {
        root["error"] = ErrorCodes::USER_EMAIL_NOT_EXISTS();
        return;  // 同上
    }
    // ... 所有错误分支都不需要手动 writeResponse()
    root["error"] = ErrorCodes::SUCCESS;
    root["host"] = reply.host();
    // ... 正常返回也自动写
});
```

`Defer` 的实现非常简单（`const.h:132-141`）：

```cpp
class Defer {
public:
    explicit Defer(std::function<void()> func) : func_(std::move(func)) {}
    ~Defer() { func_(); }  // 析构时执行
private:
    std::function<void()> func_;
};
```

> **注意**：这里 `Defer` 捕获的是 `root` 和 `connection` 的引用，不持有 shared_ptr。因为 Defer 的生命周期短于 connection（在 handler 函数内创建，函数返回前析构），所以不会产生悬空引用。

---

## 第六章：JSON 请求/响应与 gRPC 集成

### 6.1 HTTP Body 与 JSON 的双向转换

GateServer 使用 JsonCpp 处理 JSON 协议。HTTP body 到 JSON 的转换分为两步：

```cpp
// 1. Beast buffer 转 std::string
auto bodyString = boost::beast::buffers_to_string(connection->request_.body().data());

// 2. JsonCpp 解析
Json::Value srcRoot;
Json::Reader reader;
if (!reader.parse(bodyString, srcRoot)) {
    // 解析失败，返回错误
}
```

JSON 响应的构造则反过来：

```cpp
connection->response_.set(http::field::content_type, "application/json");
Json::Value root;
root["error"] = 0;
root["email"] = "alice@example.com";
boost::beast::ostream(connection->response_.body()) << root.toStyledString();
```

`beast::ostream` 是 Beast 提供的流式接口，可以方便地向 `dynamic_body` 写入内容。

### 6.2 统一错误码

项目定义了一套完整的错误码体系（`const.h:34-73`），涵盖通用、权限、业务逻辑、系统等多个类别：

```cpp
enum class ErrorCodes : int32_t {
    SUCCESS = 0,
    ERROR_REQUEST_JSON = 1001,
    RPC_FAILED = 1002,
    VERIFY_CODE_EXPIRED = 2001,
    VERIFY_CODE_NOT_REACHED = 2002,
    USER_EXISTS = 3001,
    USER_EMAIL_NOT_EXISTS = 3002,
    // ...
    RESOURCE_AUTH_FAILED = 5001,
};
```

每个 handler 的 JSON 响应中必须包含 `error` 字段，前端根据错误码展示对应提示。

### 6.3 HTTP → gRPC 的桥接

GateServer 是 HTTP 世界和 gRPC 世界的桥梁：

```
HTTP POST /get_verify_code     ──►  VerifyGrpcClient::GetVerifyCode(email)
                                        │
                                        ▼
                                  Redis 中存储验证码
                                        │
                                        ▼
                                  邮件发送 (Node.js)
                                        │
HTTP JSON Response  ◄──────────────────┘
```

后端有两个 gRPC 客户端单例：

| 客户端 | 调用方 | 用途 |
|--------|--------|------|
| `VerifyGrpcClient` | `/get_verify_code` | 请求验证码生成 |
| `StatusGrpcClient` | `/user_login` | 获取分配的 ChatServer 地址 |

登录流程涉及的调用链是最长的：

```
POST /user_login
    ├── MySQL: checkPasswd(email, passwd) → UserInfo
    ├── gRPC: StatusGrpcClient.GetChatServer(uid) → host, port, token
    └── gRPC: StatusGrpcClient.GetResourceServer(uid) → resource_host, resource_port
```

> **同步阻塞问题**：这些 gRPC 和 MySQL 调用都是同步的，在执行期间会阻塞当前 io_context 线程。在并发量不高时（网关主要是轻量请求）可以接受，但在高并发下会成为瓶颈。第七章会进一步讨论。

---

## 第七章：设计亮点、不足与改进方向

### 7.1 设计亮点

1. **多 io_context 池**：通过 `hardware_concurrency() - 1` 个 io_context 各自运行在独立线程上，实现了真正的多核并行。每个连接的读写操作绑定在同一个 io_context 上，无数据竞争。

2. **`enable_shared_from_this` 保活**：异步回调 lambda 捕获 `shared_ptr` 而不是 `this`，确保对象在异步操作期间不被析构。`GateServer`、`HttpConnection` 都使用了这一模式。

3. **handler 签名设计**：`std::function<void(shared_ptr<HttpConnection>)>` 让 LogicSystem 持有 handler，但handler 的执行者（HttpConnection）通过参数传入 shared_ptr，职责清晰 —— LogicSystem 负责分发，HttpConnection 负责 IO。

4. **Defer RAII 模式**：避免了多分支返回时遗漏 `writeResponse()` 的风险，析构自动执行收尾逻辑。

5. **配置驱动**：端口号、连接池大小等参数全部在 `config.ini` 中配置，`ConfigMgr` 单例全局访问，无需重新编译。

### 7.2 不足与改进

#### 连接模型：不支持 Keep-Alive

当前每次请求完成后直接 `shutdown_send` 关闭连接（`keep_alive(false)`），下一个 HTTP 请求必须重建 TCP 连接。在浏览器场景下（一个页面发多个 API 请求）效率很低。

**改进方向**：
- 支持 keep-alive，在 `handleRequest()` 完成后不关闭 socket，而是重新调用 `start()` 等待下一个请求
- 需要处理 HTTP/1.1 pipeline（同一个连接上的多个请求串行处理）
- 超时定时器需要调整为空闲超时而非单次请求超时

#### TIME_WAIT 累积

服务端主动 `close()` 会产生 TIME_WAIT 状态（持续 2MSL，通常 60 秒），在高并发场景下大量 TIME_WAIT 会耗尽文件描述符。

**改进方向**：
- 让客户端主动关闭（HTTP/1.1 的 Connection: close 由服务端发起）已经是当前做法
- 更好的方案是连接复用，减少关闭次数
- 可以考虑 `SO_REUSEPORT` 允许多个 accept 负载均衡

#### 同步阻塞的 handler

gRPC 和 MySQL 的同步调用会阻塞 io_context 线程。虽然多线程池可以部分缓解（一个线程阻塞不影响其他线程），但线程数有限。

**改进方向**：
- **协程（推荐）**：C++20 协程 + `boost::asio::co_spawn`，将同步阻塞调用包装为异步协程，不占用线程
- **异步回调链**：为 gRPC/MySQL 客户端提供异步接口，通过回调链组织逻辑
- **专用线程池**：创建独立的「慢操作线程池」，handler 将耗时任务 post 到该池处理，io_context 线程立即返回

#### 路由能力有限

当前路由是精确的路径匹配，不支持路径参数（`/user/:id`）、通配符、中间件（认证、日志、限流）。

**改进方向**：
- 引入正则/前缀树路由，支持参数提取
- 增加 pre-handler 中间件链（如 Token 校验）
- 考虑引入拦截器模式：preHandle → handler → postHandle

#### 错误处理

当前异常处理仅 `catch` 后打印到 stdout，没有结构化日志。

**改进方向**：
- 引入 spdlog 等日志库，分级输出
- handler 内部异常不应导致进程崩溃，但可以返回 500 + 错误 JSON

---

## 总结

GateServer 的 HTTP 实现虽然代码量不大，但涵盖了异步网络编程中的核心问题：**对象生命周期管理**（shared_ptr 保活）、**多核利用**（io_context 池）、**协议处理**（Beast 自动解析 HTTP）、**业务解耦**（路由映射表 + handler 回调）、**超时控制**（steady_timer）、**跨协议桥接**（HTTP → gRPC → MySQL/Redis）。

它不是一个通用框架，而是一个为特定场景定制的轻量网关 —— 这恰恰是大多数生产级 C++ 服务的真实形态：选对工具，按需组合，不追求大而全。

如果你也在考虑用 C++ 构建 HTTP 服务，Boost.Beast 是一个值得深入了解的选择。它的学习曲线确实比「一行代码起服务」的框架陡峭，但换来的是对底部机制的完全掌控 —— 这在排查生产问题时是无价的。
