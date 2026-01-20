# 消息路由层 (Routing Layer)

消息路由层是IM服务器的核心组件，负责：

1. 接收来自网关的消息路由请求
2. 根据消息类型和目标服务进行智能路由
3. 维护服务发现和负载均衡
4. 提供gRPC服务接口
5. 确保消息的可靠传递

## 架构设计

### 核心组件

- **RoutingService**: gRPC服务端，处理路由请求
- **MessageRouter**: 消息路由器，负责实际的路由逻辑
- **ServiceDiscovery**: 服务发现模块，管理可用服务
- **LoadBalancer**: 负载均衡模块，选择最优服务实例
- **MessageQueue**: 消息队列，处理消息积压
- **Metrics**: 监控模块，收集路由性能指标

### 目录结构

```
src/routing/
├── CMakeLists.txt              # 构建配置
├── main.cpp                    # 路由服务入口
├── RoutingService.cpp          # gRPC服务实现
├── RoutingService.h            # gRPC服务接口
├── MessageRouter.cpp           # 消息路由核心
├── MessageRouter.h             # 消息路由接口
├── ServiceDiscovery.cpp        # 服务发现
├── ServiceDiscovery.h          # 服务发现接口
├── LoadBalancer.cpp            # 负载均衡
├── LoadBalancer.h              # 负载均衡接口
├── MessageQueue.cpp            # 消息队列
├── MessageQueue.h              # 消息队列接口
├── Metrics.cpp                 # 监控模块
├── Metrics.h                   # 监控接口
├── config/                     # 配置文件
│   └── routing.conf
└── proto/                      # 协议文件
    └── gateway_routing.proto
```

## 接口设计

### gRPC服务接口

```protobuf
service RoutingService {
  // 路由消息
  rpc RouteMessage(RouteRequest) returns (RouteResponse);
  
  // 批量路由消息
  rpc BatchRouteMessages(stream RouteRequest) returns (stream RouteResponse);
  
  // 检查路由服务状态
  rpc CheckStatus(google.protobuf.Empty) returns (StatusResponse);
}
```

### 内部接口

#### MessageRouter

```cpp
class MessageRouter {
public:
  // 路由消息
  RouteResponse routeMessage(const RouteRequest& request);
  
  // 批量路由消息
  std::vector<RouteResponse> batchRouteMessages(const std::vector<RouteRequest>& requests);
  
  // 注册服务实例
  void registerService(const ServiceInstance& instance);
  
  // 移除服务实例
  void unregisterService(const std::string& service_id);
};
```

#### ServiceDiscovery

```cpp
class ServiceDiscovery {
public:
  // 获取服务实例列表
  std::vector<ServiceInstance> getServiceInstances(const std::string& service_name);
  
  // 注册服务实例
  bool registerServiceInstance(const ServiceInstance& instance);
  
  // 心跳检测
  void heartbeat();
};
```

#### LoadBalancer

```cpp
class LoadBalancer {
public:
  // 选择服务实例
  ServiceInstance selectInstance(const std::vector<ServiceInstance>& instances);
  
  // 更新服务实例状态
  void updateInstanceStatus(const ServiceInstance& instance, bool healthy);
};
```

## 工作流程

1. **消息接收**: gRPC服务端接收来自网关的路由请求
2. **消息解析**: 解析请求消息，提取关键信息
3. **服务发现**: 根据目标服务名称查找可用服务实例
4. **负载均衡**: 选择最优的服务实例
5. **消息路由**: 将消息路由到选定的服务实例
6. **响应处理**: 处理服务实例的响应，返回给网关
7. **监控统计**: 收集路由性能指标

## 技术特点

- **高性能**: 使用异步处理和线程池，支持高并发
- **可靠性**: 消息重试机制，确保消息可靠传递
- **可扩展性**: 支持服务动态注册和发现
- **可监控**: 完善的监控指标，便于运维
- **安全性**: 支持消息加密和认证

## 依赖

- **gRPC**: 远程过程调用框架
- **Protobuf**: 数据序列化
- **Boost**: 网络和线程库
- **OpenSSL**: 安全加密
- **CMake**: 构建系统

## 部署

路由服务作为独立进程部署，建议：

1. 部署多个实例，实现高可用
2. 使用负载均衡器分发请求
3. 配置服务发现机制，实现服务自动注册
4. 监控路由服务的健康状态

## 配置

主要配置项：

- **gRPC服务端口**
- **服务发现机制**
- **负载均衡策略**
- **消息队列大小**
- **线程池大小**
- **监控配置**
