# IMServer 存储模块设计文档

## 1. 模块概述

存储模块负责IMServer的数据持久化和缓存管理，支持多种存储介质，包括关系型数据库（MySQL）和键值存储（Redis）。该模块为核心模块提供数据存储和查询服务，确保数据的安全性、一致性和高性能。

## 2. 存储架构

### 2.1 分层设计

存储模块采用分层设计，分为以下几层：

- **接口层**：提供统一的存储访问接口
- **实现层**：不同存储介质的具体实现
- **缓存层**：提高数据访问性能
- **连接池层**：管理数据库连接，提高连接复用率

### 2.2 存储介质选择

| 功能 | 存储介质 | 理由 |
|------|----------|------|
| 用户信息 | MySQL | 结构化数据，需要事务支持 |
| 会话信息 | MySQL | 结构化数据，复杂查询需求 |
| 消息记录 | MySQL | 结构化数据，需要持久化存储 |
| 在线用户列表 | Redis | 高并发读写，实时性要求高 |
| 用户会话状态 | Redis | 临时数据，需要快速访问 |
| 消息队列 | Redis | 异步消息处理，提高系统性能 |
| 缓存数据 | Redis | 提高数据访问速度，减轻数据库压力 |

## 3. 核心组件设计

### 3.1 Storage接口

#### 3.1.1 功能描述
统一的存储访问接口，屏蔽不同存储介质的实现细节。

#### 3.1.2 实现细节
```cpp
class Storage {
public:
    virtual ~Storage() = default;
    
    // 用户相关接口
    virtual bool createUser(const User& user) = 0;
    virtual bool getUserByUsername(const std::string& username, User& out_user) = 0;
    virtual bool getUserById(uint64_t user_id, User& out_user) = 0;
    virtual bool updateUser(const User& user) = 0;
    
    // 会话相关接口
    virtual bool createSession(const Session& session) = 0;
    virtual bool getSessionById(uint64_t session_id, Session& out_session) = 0;
    virtual bool getUserSessions(uint64_t user_id, std::vector<Session>& out_sessions) = 0;
    
    // 消息相关接口
    virtual bool createMessage(const Message& message) = 0;
    virtual bool getSessionMessages(uint64_t session_id, uint64_t last_message_id, 
                                   size_t limit, std::vector<Message>& out_messages) = 0;
    virtual bool updateMessageStatus(uint64_t message_id, uint64_t user_id, uint32_t status) = 0;
    
    // 事务接口
    virtual bool beginTransaction() = 0;
    virtual bool commitTransaction() = 0;
    virtual bool rollbackTransaction() = 0;
};
```

### 3.2 MySQLStorage

#### 3.2.1 功能描述
MySQL存储实现，负责结构化数据的持久化存储。

#### 3.2.2 实现细节
- 基于MySQL C++ Connector
- 支持事务处理
- 连接池管理
- 预处理语句
- 错误重试机制

```cpp
class MySQLStorage : public Storage {
private:
    std::shared_ptr<MySQLConnectionPool> connection_pool_;

public:
    MySQLStorage(const std::string& host, uint16_t port, const std::string& database, 
                const std::string& username, const std::string& password, 
                size_t pool_size = 10);
    
    // 用户相关接口实现
    bool createUser(const User& user) override;
    bool getUserByUsername(const std::string& username, User& out_user) override;
    bool getUserById(uint64_t user_id, User& out_user) override;
    bool updateUser(const User& user) override;
    
    // 会话相关接口实现
    bool createSession(const Session& session) override;
    bool getSessionById(uint64_t session_id, Session& out_session) override;
    bool getUserSessions(uint64_t user_id, std::vector<Session>& out_sessions) override;
    
    // 消息相关接口实现
    bool createMessage(const Message& message) override;
    bool getSessionMessages(uint64_t session_id, uint64_t last_message_id, 
                           size_t limit, std::vector<Message>& out_messages) override;
    bool updateMessageStatus(uint64_t message_id, uint64_t user_id, uint32_t status) override;
    
    // 事务接口实现
    bool beginTransaction() override;
    bool commitTransaction() override;
    bool rollbackTransaction() override;
};
```

### 3.3 RedisStorage

#### 3.3.1 功能描述
Redis存储实现，负责缓存数据和临时数据的存储。

#### 3.3.2 实现细节
- 基于hiredis库
- 支持字符串、哈希、列表、集合等数据结构
- 发布订阅功能
- 过期时间设置
- 管道操作

```cpp
class RedisStorage {
private:
    redisContext* context_;
    
    // 连接Redis服务器
    bool connect(const std::string& host, uint16_t port, const std::string& password = "", int db = 0);
    
    // 执行Redis命令
    redisReply* executeCommand(const char* format, ...);

public:
    RedisStorage(const std::string& host, uint16_t port, const std::string& password = "", int db = 0);
    ~RedisStorage();
    
    // 字符串操作
    bool set(const std::string& key, const std::string& value, time_t expire_seconds = 0);
    bool get(const std::string& key, std::string& out_value);
    
    // 哈希操作
    bool hset(const std::string& key, const std::string& field, const std::string& value);
    bool hget(const std::string& key, const std::string& field, std::string& out_value);
    bool hgetAll(const std::string& key, std::unordered_map<std::string, std::string>& out_fields);
    
    // 列表操作
    bool lpush(const std::string& key, const std::vector<std::string>& values);
    bool rpop(const std::string& key, std::string& out_value);
    bool lrange(const std::string& key, int start, int end, std::vector<std::string>& out_values);
    
    // 集合操作
    bool sadd(const std::string& key, const std::vector<std::string>& values);
    bool srem(const std::string& key, const std::vector<std::string>& values);
    bool smembers(const std::string& key, std::vector<std::string>& out_members);
    
    // 发布订阅
    bool publish(const std::string& channel, const std::string& message);
    
    // 键操作
    bool exists(const std::string& key);
    bool del(const std::string& key);
    bool expire(const std::string& key, time_t seconds);
};
```

### 3.4 MySQLConnectionPool

#### 3.4.1 功能描述
MySQL连接池，管理数据库连接的创建、复用和回收。

#### 3.4.2 实现细节
- 预创建一定数量的数据库连接
- 连接空闲超时回收
- 动态调整连接池大小
- 线程安全设计

```cpp
class MySQLConnectionPool {
private:
    struct Connection {
        MYSQL* mysql;
        time_t last_used_time;
        bool in_use;
    };
    
    std::string host_;
    uint16_t port_;
    std::string database_;
    std::string username_;
    std::string password_;
    
    size_t min_pool_size_;
    size_t max_pool_size_;
    size_t current_pool_size_;
    
    std::vector<std::unique_ptr<Connection>> connections_;
    std::mutex connections_mutex_;
    std::condition_variable cv_;
    
    // 创建新连接
    std::unique_ptr<Connection> createConnection();
    
    // 验证连接是否有效
    bool validateConnection(MYSQL* mysql);

public:
    MySQLConnectionPool(const std::string& host, uint16_t port, const std::string& database, 
                       const std::string& username, const std::string& password, 
                       size_t min_pool_size = 5, size_t max_pool_size = 20);
    
    ~MySQLConnectionPool();
    
    // 获取连接
    std::unique_ptr<Connection> getConnection(time_t timeout_seconds = 5);
    
    // 归还连接
    void returnConnection(std::unique_ptr<Connection> conn);
    
    // 关闭所有连接
    void closeAllConnections();
};
```

### 3.5 CacheManager

#### 3.5.1 功能描述
缓存管理器，负责数据的缓存策略和缓存更新。

#### 3.5.2 实现细节
- 基于Redis的缓存实现
- 支持LRU缓存策略
- 缓存过期机制
- 缓存一致性保证

```cpp
class CacheManager {
private:
    std::shared_ptr<RedisStorage> redis_;
    
public:
    CacheManager(std::shared_ptr<RedisStorage> redis);
    
    // 用户信息缓存
    bool cacheUser(const User& user, time_t expire_seconds = 3600);
    bool getCachedUser(uint64_t user_id, User& out_user);
    bool invalidateUserCache(uint64_t user_id);
    
    // 会话信息缓存
    bool cacheSession(const Session& session, time_t expire_seconds = 3600);
    bool getCachedSession(uint64_t session_id, Session& out_session);
    bool invalidateSessionCache(uint64_t session_id);
    
    // 在线用户缓存
    bool addOnlineUser(uint64_t user_id, const std::string& client_info);
    bool removeOnlineUser(uint64_t user_id);
    bool isUserOnline(uint64_t user_id);
    std::vector<uint64_t> getOnlineUsers();
    
    // 消息缓存
    bool cacheMessage(const Message& message, time_t expire_seconds = 600);
    bool getCachedMessage(uint64_t message_id, Message& out_message);
};
```

## 4. 数据模型

### 4.1 用户表 (users)

| 字段名 | 数据类型 | 约束 | 描述 |
|--------|----------|------|------|
| user_id | BIGINT | PRIMARY KEY, AUTO_INCREMENT | 用户ID |
| username | VARCHAR(50) | UNIQUE, NOT NULL | 用户名 |
| password_hash | VARCHAR(64) | NOT NULL | 密码哈希 |
| nickname | VARCHAR(50) | NOT NULL | 昵称 |
| avatar_url | VARCHAR(255) | | 头像URL |
| status | INT | DEFAULT 0 | 状态（0:离线, 1:在线, 2:忙碌, 3:离开） |
| last_login_time | DATETIME | | 最后登录时间 |
| created_time | DATETIME | NOT NULL | 创建时间 |
| updated_time | DATETIME | NOT NULL | 更新时间 |

### 4.2 会话表 (sessions)

| 字段名 | 数据类型 | 约束 | 描述 |
|--------|----------|------|------|
| session_id | BIGINT | PRIMARY KEY, AUTO_INCREMENT | 会话ID |
| type | INT | NOT NULL | 会话类型（0:单聊, 1:群聊） |
| created_time | DATETIME | NOT NULL | 创建时间 |
| updated_time | DATETIME | NOT NULL | 更新时间 |

### 4.3 单聊会话表 (chat_sessions)

| 字段名 | 数据类型 | 约束 | 描述 |
|--------|----------|------|------|
| session_id | BIGINT | PRIMARY KEY, FOREIGN KEY | 会话ID |
| user_id1 | BIGINT | NOT NULL, FOREIGN KEY | 用户ID1 |
| user_id2 | BIGINT | NOT NULL, FOREIGN KEY | 用户ID2 |

### 4.4 群聊会话表 (group_sessions)

| 字段名 | 数据类型 | 约束 | 描述 |
|--------|----------|------|------|
| session_id | BIGINT | PRIMARY KEY, FOREIGN KEY | 会话ID |
| group_id | BIGINT | NOT NULL, FOREIGN KEY | 群ID |

### 4.5 群表 (groups)

| 字段名 | 数据类型 | 约束 | 描述 |
|--------|----------|------|------|
| group_id | BIGINT | PRIMARY KEY, AUTO_INCREMENT | 群ID |
| group_name | VARCHAR(100) | NOT NULL | 群名称 |
| creator_id | BIGINT | NOT NULL, FOREIGN KEY | 创建者ID |
| created_time | DATETIME | NOT NULL | 创建时间 |
| updated_time | DATETIME | NOT NULL | 更新时间 |

### 4.6 群成员表 (group_members)

| 字段名 | 数据类型 | 约束 | 描述 |
|--------|----------|------|------|
| group_id | BIGINT | NOT NULL, FOREIGN KEY | 群ID |
| user_id | BIGINT | NOT NULL, FOREIGN KEY | 用户ID |
| role | INT | DEFAULT 0 | 角色（0:成员, 1:管理员, 2:群主） |
| joined_time | DATETIME | NOT NULL | 加入时间 |
| PRIMARY KEY | (group_id, user_id) | | 联合主键 |

### 4.7 消息表 (messages)

| 字段名 | 数据类型 | 约束 | 描述 |
|--------|----------|------|------|
| message_id | BIGINT | PRIMARY KEY, AUTO_INCREMENT | 消息ID |
| session_id | BIGINT | NOT NULL, FOREIGN KEY | 会话ID |
| type | INT | NOT NULL | 消息类型（0:文本, 1:图片, 2:语音, 3:视频, 4:文件） |
| sender_id | BIGINT | NOT NULL, FOREIGN KEY | 发送者ID |
| content | TEXT | NOT NULL | 消息内容 |
| created_time | DATETIME | NOT NULL | 创建时间 |

### 4.8 消息状态表 (message_status)

| 字段名 | 数据类型 | 约束 | 描述 |
|--------|----------|------|------|
| message_id | BIGINT | NOT NULL, FOREIGN KEY | 消息ID |
| user_id | BIGINT | NOT NULL, FOREIGN KEY | 用户ID |
| status | INT | NOT NULL | 消息状态（0:发送中, 1:已发送, 2:已接收, 3:已读） |
| updated_time | DATETIME | NOT NULL | 更新时间 |
| PRIMARY KEY | (message_id, user_id) | | 联合主键 |

## 5. 性能优化

### 5.1 数据库优化

- 使用索引优化查询性能
- 合理设计表结构，避免冗余数据
- 使用分表分库技术处理大数据量
- 定期优化数据库表和索引

### 5.2 缓存优化

- 热点数据缓存
- 合理设置缓存过期时间
- 使用缓存预热技术
- 实现缓存穿透、缓存击穿和缓存雪崩的防护

### 5.3 连接池优化

- 根据系统负载动态调整连接池大小
- 设置合理的连接超时时间
- 定期清理无效连接
- 监控连接池使用情况

## 6. 错误处理与恢复

### 6.1 错误处理策略

- 捕获并记录所有数据库错误
- 实现错误重试机制
- 提供友好的错误提示
- 定期备份数据，确保数据安全

### 6.2 数据恢复

- 定期数据备份
- 主从复制架构，提高系统可用性
- 故障转移机制
- 数据恢复流程

## 7. 安全性设计

### 7.1 数据加密

- 敏感数据加密存储
- 数据库连接加密
- 数据传输加密

### 7.2 访问控制

- 最小权限原则
- 数据库用户权限管理
- 防止SQL注入攻击
- 防止恶意访问

### 7.3 审计日志

- 记录所有数据库操作
- 定期审计数据库访问
- 检测异常访问行为

## 8. 集成与使用

### 8.1 模块初始化

```cpp
// 创建MySQL存储实例
std::string mysql_host = "localhost";
uint16_t mysql_port = 3306;
std::string mysql_database = "imserver";
std::string mysql_username = "root";
std::string mysql_password = "password";

auto mysql_storage = std::make_shared<MySQLStorage>(mysql_host, mysql_port, mysql_database, 
                                                    mysql_username, mysql_password);

// 创建Redis存储实例
std::string redis_host = "localhost";
uint16_t redis_port = 6379;
std::string redis_password = "";
int redis_db = 0;

auto redis_storage = std::make_shared<RedisStorage>(redis_host, redis_port, redis_password, redis_db);

// 创建缓存管理器
auto cache_manager = std::make_shared<CacheManager>(redis_storage);
```

### 8.2 数据存储示例

```cpp
// 创建用户
User user;
user.username = "testuser";
user.password_hash = "hash123";
user.nickname = "Test User";
user.status = 1; // 在线

// 存储用户信息
if (mysql_storage->createUser(user)) {
    // 用户创建成功，缓存用户信息
    cache_manager->cacheUser(user);
    std::cout << "User created successfully" << std::endl;
} else {
    std::cerr << "Failed to create user" << std::endl;
}
```

### 8.3 数据查询示例

```cpp
// 查询用户信息
User user;
std::string username = "testuser";

// 先从缓存查询
if (cache_manager->getCachedUser(user.user_id, user)) {
    std::cout << "User found in cache: " << user.username << std::endl;
} else {
    // 缓存未命中，从数据库查询
    if (mysql_storage->getUserByUsername(username, user)) {
        // 查询成功，更新缓存
        cache_manager->cacheUser(user);
        std::cout << "User found in database: " << user.username << std::endl;
    } else {
        std::cerr << "User not found" << std::endl;
    }
}
```

## 9. 未来扩展

- 支持更多存储介质（MongoDB、Elasticsearch等）
- 实现分布式存储架构
- 添加数据分片功能
- 实现数据同步机制
- 添加数据压缩功能

## 10. 总结

存储模块采用分层设计，提供统一的存储访问接口，支持MySQL和Redis等多种存储介质。通过连接池和缓存机制，提高了数据访问性能和系统可靠性。该模块为IMServer提供了高效、可靠的数据存储服务，支持用户管理、会话管理和消息处理等核心功能。