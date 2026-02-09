# C++ 客户端管理不同 grpc 服务

**一个目标地址（Target）对应一个共享的 gRPC Channel 连接**，同地址的所有服务 Stub 复用该 Channel；Stub 是轻量级无状态对象，可基于共享 Channel 随时创建，无需单独管理。

## 核心架构

采用**统一配置 + 单例 Channel 池 + 服务工厂 + 业务客户端封装**四层架构，解耦资源管理与业务调用。

```plaintext
配置管理（地址/超时/保活）→ Channel池（单例，按Target管理）→ 服务工厂（创建Stub，绑定Channel）→ 业务客户端（封装调用、异常、元数据）
```

各层职责清晰，底层负责连接复用和资源安全，上层专注业务逻辑，完全符合开闭原则（新增服务无需修改底层代码）。

## 模块设计

### 服务配置管理

集中管理所有 gRPC 服务的配置，一个服务标识对应一组配置（Target 地址、超时、保活等），避免硬编码，支持后续对接配置中心（如 Nacos/ETCD）。

```cpp
struct ServiceConfig {
    std::string name;
    std::string id;
    std::string target;
    std::chrono::milliseconds timeout; // 默认调用超时（毫秒）
    int keepalive_time_s;              // 长连接保活时间（秒）
    int keepalive_timeout_s;           // 保活包超时时间（秒）
};

class ServiceInstance {
public:
    ServiceConfig config;
    bool healthy;
    int load;
    
    ServiceInstance() : port(0), healthy(false), load(0) {}
    ServiceInstance(ServiceConfig config) : config(config), healthy(false), load(0) {}
    ServiceInstance(ServiceConfig &&config) : config(std::move(config)), healthy(false), load(0) {}
};
```

### 单例 Channel 池

实现线程安全的单例 Channel 池，按Target作为唯一 Key，保证一个地址仅创建一个 Channel；提供「获取 Channel、关闭所有 Channel」方法，避免连接泄漏和重复创建，是多服务管理的核心。

```cpp
// 单例Channel池：管理所有gRPC Channel，按Target唯一标识
class ServiceChannelPool {
public:
    // 单例模式：获取唯一实例（C++11静态局部变量保证线程安全）
    static ServiceChannelPool& Instance();

    // 获取指定Target的Channel（不存在则创建，线程安全）
    std::shared_ptr<grpc::Channel> GetChannel(const std::string& target, const GrpcServiceConfig& cfg);

    // 关闭所有Channel（程序退出时调用，释放资源）
    void CloseAll();

    // 禁止拷贝和赋值（单例核心）
    ServiceChannelPool(const ServiceChannelPool&) = delete;
    ServiceChannelPool& operator=(const ServiceChannelPool&) = delete;

private:
    // 私有构造/析构：仅能通过Instance()获取实例
    ServiceChannelPool() = default;
    ~ServiceChannelPool() = default;

    // Channel映射：key=Target，value=共享指针管理的Channel
    std::unordered_map<std::string, std::shared_ptr<grpc::Channel>> channel_map_;
    // 读写互斥锁：保证并发访问安全（读多写少，可后续优化为读写锁）
    std::mutex mtx_;

    // 构建Channel的配置选项（基于服务配置）
    grpc::ChannelArguments BuildChannelArgs(const GrpcServiceConfig& cfg);
};
```

### 服务工厂

基于共享 Channel创建对应服务的 Stub（存根），Stub 是 gRPC 服务的调用入口，C++ 中 Stub 为轻量级无状态对象，创建成本极低，可随用随建（无需缓存）。

