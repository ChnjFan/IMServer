# IMServer 核心模块设计文档

## 1. 模块概述

核心模块是IMServer的业务逻辑中心，负责处理用户管理、会话管理、消息处理等核心功能。该模块与网络模块、存储模块等其他模块紧密协作，实现完整的即时通讯功能。

## 2. 核心组件设计

### 2.1 UserManager

#### 2.1.1 功能描述
用户管理器负责用户的注册、登录、认证和信息管理等功能。

#### 2.1.2 数据结构
```cpp
struct User {
    uint64_t user_id;
    std::string username;
    std::string password_hash;
    std::string nickname;
    std::string avatar_url;
    uint32_t status; // 0: 离线, 1: 在线, 2: 忙碌, 3: 离开
    time_t last_login_time;
    time_t created_time;
    time_t updated_time;
};
```

#### 2.1.3 实现细节
- 提供用户注册、登录、注销功能
- 密码加密存储（使用SHA-256）
- 用户身份验证
- 在线状态管理
- 用户信息查询和更新

```cpp
class UserManager {
private:
    std::shared_ptr<Storage> storage_;
    std::unordered_map<uint64_t, User> online_users_;
    std::mutex online_users_mutex_;

public:
    UserManager(std::shared_ptr<Storage> storage);
    
    // 用户注册
    bool registerUser(const std::string& username, const std::string& password, 
                     const std::string& nickname, User& out_user);
    
    // 用户登录
    bool loginUser(const std::string& username, const std::string& password, 
                  User& out_user, std::string& out_token);
    
    // 用户注销
    bool logoutUser(uint64_t user_id);
    
    // 验证用户令牌
    bool validateToken(uint64_t user_id, const std::string& token);
    
    // 更新用户状态
    bool updateUserStatus(uint64_t user_id, uint32_t status);
    
    // 获取用户信息
    bool getUserInfo(uint64_t user_id, User& out_user);
    
    // 获取在线用户列表
    std::vector<User> getOnlineUsers();
};
```

### 2.2 SessionManager

#### 2.2.1 功能描述
会话管理器负责管理用户之间的会话关系，包括单聊会话和群聊会话。

#### 2.2.2 数据结构
```cpp
struct Session {
    uint64_t session_id;
    uint32_t type; // 0: 单聊, 1: 群聊
    time_t created_time;
    time_t updated_time;
};

struct ChatSession : public Session {
    uint64_t user_id1;
    uint64_t user_id2;
};

struct GroupSession : public Session {
    uint64_t group_id;
};
```

#### 2.2.3 实现细节
- 创建和管理单聊会话
- 创建和管理群聊会话
- 会话成员管理
- 会话历史记录查询

```cpp
class SessionManager {
private:
    std::shared_ptr<Storage> storage_;
    
public:
    SessionManager(std::shared_ptr<Storage> storage);
    
    // 创建单聊会话
    bool createChatSession(uint64_t user_id1, uint64_t user_id2, uint64_t& out_session_id);
    
    // 创建群聊会话
    bool createGroupSession(const std::string& group_name, const std::vector<uint64_t>& member_ids, 
                           uint64_t& out_session_id, uint64_t& out_group_id);
    
    // 获取会话信息
    bool getSessionInfo(uint64_t session_id, Session& out_session);
    
    // 获取用户的会话列表
    std::vector<Session> getUserSessions(uint64_t user_id);
    
    // 添加群成员
    bool addGroupMember(uint64_t group_id, uint64_t user_id);
    
    // 移除群成员
    bool removeGroupMember(uint64_t group_id, uint64_t user_id);
    
    // 获取群成员列表
    std::vector<uint64_t> getGroupMembers(uint64_t group_id);
};
```

### 2.3 MessageManager

#### 2.3.1 功能描述
消息管理器负责消息的发送、接收、存储和查询等功能。

#### 2.3.2 数据结构
```cpp
struct Message {
    uint64_t message_id;
    uint64_t session_id;
    uint32_t type; // 0: 文本, 1: 图片, 2: 语音, 3: 视频, 4: 文件
    uint64_t sender_id;
    std::string content;
    uint32_t status; // 0: 发送中, 1: 已发送, 2: 已接收, 3: 已读
    time_t created_time;
};
```

#### 2.3.3 实现细节
- 支持多种消息类型（文本、图片、语音、视频、文件）
- 消息发送和接收
- 消息状态跟踪（发送中、已发送、已接收、已读）
- 消息历史记录查询
- 未读消息计数

```cpp
class MessageManager {
private:
    std::shared_ptr<Storage> storage_;
    std::shared_ptr<Network> network_;
    
public:
    MessageManager(std::shared_ptr<Storage> storage, std::shared_ptr<Network> network);
    
    // 发送消息
    bool sendMessage(const Message& message, std::vector<uint64_t>& out_received_users);
    
    // 消息已接收确认
    bool confirmMessageReceived(uint64_t message_id, uint64_t user_id);
    
    // 消息已读确认
    bool confirmMessageRead(uint64_t message_id, uint64_t user_id);
    
    // 获取会话消息历史
    std::vector<Message> getSessionMessages(uint64_t session_id, uint64_t last_message_id, size_t limit);
    
    // 获取未读消息计数
    uint32_t getUnreadMessageCount(uint64_t user_id, uint64_t session_id);
    
    // 获取所有未读消息
    std::vector<Message> getAllUnreadMessages(uint64_t user_id);
};
```

### 2.4 MessageRouter

#### 2.4.1 功能描述
消息路由器负责将消息从发送方路由到接收方，支持单播、组播等多种消息传递方式。

#### 2.4.2 实现细节
- 消息路由算法
- 支持单聊、群聊消息传递
- 离线消息处理
- 消息转发

```cpp
class MessageRouter {
private:
    std::shared_ptr<ConnectionManager> conn_manager_;
    
public:
    MessageRouter(std::shared_ptr<ConnectionManager> conn_manager);
    
    // 路由消息到指定用户
    bool routeToUser(uint64_t user_id, const std::vector<char>& data);
    
    // 路由消息到多个用户
    bool routeToUsers(const std::vector<uint64_t>& user_ids, const std::vector<char>& data);
    
    // 路由消息到群聊
    bool routeToGroup(uint64_t group_id, const std::vector<char>& data, uint64_t exclude_user_id = 0);
};
```

### 2.5 ConnectionManager

#### 2.5.1 功能描述
连接管理器负责管理客户端连接，维护连接与用户的映射关系。

#### 2.5.2 实现细节
- 管理所有客户端连接
- 维护用户ID与连接的映射
- 连接生命周期管理
- 连接状态监控

```cpp
class ConnectionManager {
private:
    std::unordered_map<uint64_t, Network::Connection::Ptr> user_connections_;
    std::unordered_map<Network::Connection::Ptr, uint64_t> connection_users_;
    std::mutex connections_mutex_;
    
public:
    ConnectionManager();
    
    // 添加连接
    void addConnection(Network::Connection::Ptr conn);
    
    // 移除连接
    void removeConnection(Network::Connection::Ptr conn);
    
    // 关联用户与连接
    bool associateUserWithConnection(uint64_t user_id, Network::Connection::Ptr conn);
    
    // 取消用户与连接的关联
    bool disassociateUserWithConnection(uint64_t user_id);
    
    // 获取用户的连接
    Network::Connection::Ptr getUserConnection(uint64_t user_id);
    
    // 获取连接对应的用户
    bool getConnectionUser(Network::Connection::Ptr conn, uint64_t& out_user_id);
    
    // 获取所有连接数
    size_t getConnectionCount();
    
    // 广播消息
    void broadcast(const std::vector<char>& data);
};
```

## 3. 业务流程

### 3.1 用户登录流程

1. 客户端发送登录请求（用户名、密码）
2. 网络模块接收请求并转发给核心模块
3. UserManager验证用户身份
4. 生成登录令牌
5. 更新用户状态为在线
6. 关联用户与连接
7. 返回登录成功响应

### 3.2 单聊消息发送流程

1. 客户端发送单聊消息请求
2. 网络模块接收请求并转发给核心模块
3. MessageManager验证发送者身份
4. 创建消息记录
5. MessageRouter将消息路由到接收方
6. 更新消息状态为已发送
7. 返回发送成功响应
8. 接收方确认接收消息
9. 更新消息状态为已接收
10. 接收方确认已读消息
11. 更新消息状态为已读

### 3.3 群聊消息发送流程

1. 客户端发送群聊消息请求
2. 网络模块接收请求并转发给核心模块
3. MessageManager验证发送者身份和群成员身份
4. 创建消息记录
5. 获取群成员列表
6. MessageRouter将消息路由到所有群成员
7. 更新消息状态为已发送
8. 返回发送成功响应

## 4. 错误处理

核心模块采用统一的错误处理机制：

```cpp
enum class ErrorCode {
    SUCCESS = 0,
    
    // 用户相关错误
    USER_NOT_FOUND = 1001,
    USERNAME_EXISTS = 1002,
    INVALID_PASSWORD = 1003,
    USER_ALREADY_ONLINE = 1004,
    
    // 会话相关错误
    SESSION_NOT_FOUND = 2001,
    USER_NOT_IN_GROUP = 2002,
    
    // 消息相关错误
    MESSAGE_SEND_FAILED = 3001,
    MESSAGE_NOT_FOUND = 3002,
    
    // 其他错误
    INVALID_PARAMETER = 9001,
    DATABASE_ERROR = 9002,
    INTERNAL_ERROR = 9999
};
```

每个函数都返回错误码和详细的错误信息，便于调用者进行错误处理和日志记录。

## 5. 安全性设计

### 5.1 身份认证
- 用户登录时进行身份验证
- 使用JSON Web Token (JWT) 进行会话管理
- 令牌定期过期，需要重新登录

### 5.2 数据加密
- 密码使用SHA-256哈希存储
- 敏感数据在传输过程中加密
- 数据库存储加密

### 5.3 访问控制
- 验证用户是否有权限访问资源
- 限制单用户的并发连接数
- 防止SQL注入和XSS攻击

## 6. 性能优化

### 6.1 缓存机制
- 在线用户信息缓存
- 热门会话缓存
- 频繁访问的用户信息缓存

### 6.2 异步处理
- 耗时操作异步执行
- 使用线程池处理并发请求

### 6.3 批量操作
- 批量消息发送
- 批量数据库操作

## 7. 集成与使用

### 7.1 模块初始化

```cpp
// 创建存储模块实例
auto storage = std::make_shared<Storage>();

// 创建核心模块实例
auto user_manager = std::make_shared<UserManager>(storage);
auto session_manager = std::make_shared<SessionManager>(storage);
auto connection_manager = std::make_shared<ConnectionManager>();
auto message_router = std::make_shared<MessageRouter>(connection_manager);
auto message_manager = std::make_shared<MessageManager>(storage, network);
```

### 7.2 网络事件处理

```cpp
// 消息处理回调
network->setMessageHandler([&](Network::Connection::Ptr conn, const std::vector<char>& data) {
    // 解析协议
    auto message = Protocol::parse(data);
    
    // 根据消息类型处理
    switch (message.type) {
        case Protocol::MessageType::LOGIN_REQUEST:
            // 处理登录请求
            handleLoginRequest(conn, message);
            break;
        case Protocol::MessageType::CHAT_MESSAGE:
            // 处理聊天消息
            handleChatMessage(conn, message);
            break;
        // 其他消息类型处理
    }
});
```

## 8. 未来扩展

核心模块的未来扩展计划：

- 添加好友关系管理功能
- 实现消息撤回功能
- 添加消息搜索功能
- 支持多设备登录
- 实现消息漫游功能
- 添加用户封禁和黑名单功能

## 9. 总结

核心模块是IMServer的业务逻辑核心，实现了用户管理、会话管理、消息处理等核心功能。该模块采用模块化、高内聚、低耦合的设计原则，便于扩展和维护。通过与网络模块、存储模块等其他模块的协作，实现了完整的即时通讯功能。