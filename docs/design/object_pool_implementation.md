# ObjectPool 实现文档

## 概述

ObjectPool是一个通用的对象池模板类，用于复用对象并减少动态内存分配的开销。该实现位于`src/tool/`目录下，提供了高性能的内存管理和对象复用机制。

## 文件结构

```
src/tool/
├── ObjectPool.h          // ObjectPool类头文件
├── ObjectPool.tpp        // ObjectPool类模板实现
├── ObjectPoolTest.cpp    // 测试文件
└── CMakeLists.txt        // 构建配置
```

## 核心特性

### 1. 通用模板设计
- 支持任意类型的对象（T）
- 要求对象支持默认构造函数
- 使用智能指针（std::shared_ptr）管理生命周期

### 2. 内存管理
- **初始大小配置**: 可设置对象池的初始对象数量
- **最大容量限制**: 可设置对象池的最大对象数量
- **自动扩容**: 当对象不足时自动创建新对象
- **智能收缩**: 当对象过多时自动释放不用的对象

### 3. 线程安全
- 使用互斥锁（std::mutex）保护共享资源
- 支持多线程并发访问
- 原子操作（std::atomic）处理统计数据

### 4. 回调机制
- **构造回调**: 对象创建时的自定义初始化
- **重置回调**: 对象归还时的状态重置

### 5. 统计信息
- 总对象数、可用对象数、已借出对象数
- 获取次数、归还次数、失败次数
- 最后活动时间

## API 参考

### 构造函数

```cpp
explicit ObjectPool(
    size_t initial_size = 10,          // 初始对象数
    size_t max_size = 100,             // 最大对象数
    bool enable_auto_expand = true,    // 启用自动扩容
    size_t expand_step = 10,           // 扩容步长
    size_t shrink_threshold = 50       // 收缩阈值
);
```

### 核心方法

#### 获取对象
```cpp
template<typename... Args>
std::shared_ptr<T> acquire(Args&&... args);
```
从对象池中获取对象。如果没有可用对象且未达到最大数量，将创建新对象。

#### 归还对象
```cpp
void release(std::shared_ptr<T> object);
```
将对象归还到对象池中。会自动调用重置回调函数并放入可用队列。

#### 预热对象池
```cpp
template<typename... Args>
void warmup(size_t count, Args&&... args);
```
预先创建指定数量的对象，减少运行时创建对象的开销。

#### 清理对象池
```cpp
void clear();
```
清理对象池中的所有对象，但不影响已经借出的对象。

#### 获取统计信息
```cpp
PoolStats getStats() const;
```
返回对象池的详细统计信息。

### 回调设置

#### 设置重置回调
```cpp
void setResetCallback(std::function<void(T&)> reset_func);
```
设置对象归还时的重置回调函数。

#### 设置构造回调
```cpp
void setConstructCallback(std::function<void(T&)> construct_func);
```
设置对象创建时的构造回调函数。

## 使用示例

### 基本用法

```cpp
#include "ObjectPool.h"

using namespace imserver::tool;

// 定义一个需要复用的类
class Connection {
public:
    Connection() : connected_(false) {}
    
    void connect(const std::string& host, int port) {
        connected_ = true;
        host_ = host;
        port_ = port;
    }
    
    void disconnect() {
        connected_ = false;
        host_.clear();
        port_ = 0;
    }
    
    bool isConnected() const { return connected_; }
    
    void reset() {
        disconnect();
    }
    
private:
    bool connected_;
    std::string host_;
    int port_;
};

int main() {
    // 创建连接对象池
    ObjectPool<Connection> pool(5, 20, true, 5, 15);
    
    // 获取连接
    auto conn = pool.acquire();
    conn->connect("127.0.0.1", 8080);
    
    // 使用连接...
    
    // 归还连接
    pool.release(conn);
    
    return 0;
}
```

### 高级用法：回调函数

```cpp
int main() {
    ObjectPool<Connection> pool(10, 100);
    
    // 设置重置回调
    pool.setResetCallback([](Connection& conn) {
        conn.reset();
        std::cout << "连接已重置" << std::endl;
    });
    
    // 设置构造回调
    pool.setConstructCallback([](Connection& conn) {
        std::cout << "新连接已创建" << std::endl;
    });
    
    auto conn = pool.acquire();
    // 构造回调会被调用
    
    pool.release(conn);
    // 重置回调会被调用
    
    return 0;
}
```

### 监控统计信息

```cpp
int main() {
    ObjectPool<Connection> pool(10, 100);
    
    // 获取一些连接并归还
    std::vector<decltype(pool.acquire())> connections;
    for (int i = 0; i < 50; ++i) {
        connections.push_back(pool.acquire());
    }
    
    // 获取统计信息
    auto stats = pool.getStats();
    std::cout << "总对象数: " << stats.total_objects << std::endl;
    std::cout << "可用对象数: " << stats.available_objects << std::endl;
    std::cout << "已借出对象数: " << stats.acquired_objects << std::endl;
    std::cout << "获取次数: " << stats.total_acquisitions << std::endl;
    std::cout << "归还次数: " << stats.total_releases << std::endl;
    std::cout << "失败次数: " << stats.failed_acquisitions << std::endl;
    
    return 0;
}
```

## 性能优化建议

### 1. 合理设置初始大小
根据应用的并发负载，设置合适的初始对象数量：
- 高并发应用：初始大小设置为预期的并发数
- 低并发应用：初始大小设置为较小的值

### 2. 调整最大容量
根据系统内存和性能要求，设置合理的最大容量：
- 内存充足：可以设置较大的最大容量
- 内存紧张：需要严格控制最大容量

### 3. 优化回调函数
- 回调函数应该轻量级，避免复杂的操作
- 避免在回调函数中抛出异常
- 使用常量引用传递参数，避免不必要的拷贝

### 4. 监控池状态
定期检查池的统计信息，及时调整配置参数：
- 如果失败次数过多，考虑增大最大容量
- 如果可用对象数过少，考虑增大初始大小

## 错误处理

ObjectPool在以下情况下会抛出异常：
1. 池已满且无法扩容时调用`acquire()`
2. 扩容过程中创建对象失败
3. 回调函数执行失败（不会中断归还流程）

```cpp
try {
    auto obj = pool.acquire();
    // 使用对象...
    pool.release(obj);
} catch (const std::runtime_error& e) {
    std::cerr << "对象池错误: " << e.what() << std::endl;
}
```

## 与Connection管理器的集成

ObjectPool可以与统一连接管理系统集成，为Connection对象提供复用机制：

```cpp
// 在ConnectionManager中使用ObjectPool
class ConnectionManager {
private:
    ObjectPool<Connection> connection_pool_;
    
public:
    ConnectionManager() 
        : connection_pool_(10, 1000, true, 50, 800) {
        
        // 设置重置回调
        connection_pool_.setResetCallback([](Connection& conn) {
            conn.reset();
        });
    }
    
    std::shared_ptr<Connection> createConnection(ConnectionId id, ConnectionType type) {
        auto conn = connection_pool_.acquire(id, type);
        return conn;
    }
    
    void destroyConnection(std::shared_ptr<Connection> conn) {
        connection_pool_.release(conn);
    }
};
```

## 构建和测试

### 构建项目
```bash
# 在项目根目录执行
mkdir -p build && cd build
cmake ..
make ObjectPoolTest
```

### 运行测试
```bash
# 运行测试程序
./test/ObjectPoolTest
```

测试程序包含以下测试用例：
1. 基本功能测试：获取、归还、复用对象
2. 并发访问测试：多线程环境下的安全性
3. 回调函数测试：构造和重置回调的执行
4. 异常处理测试：池满时的异常处理

## 总结

ObjectPool实现提供了一个高效、安全、易用的对象复用机制，特别适用于：
- 网络连接管理
- 内存密集型应用
- 高频创建销毁对象的场景
- 需要控制内存使用的应用

通过合理配置和监控，ObjectPool可以显著提升应用的性能和内存使用效率。