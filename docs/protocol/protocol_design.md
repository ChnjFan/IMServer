# IMServer 协议设计文档

## 1. 协议概述

IMServer支持多种通信协议，以满足不同客户端的需求：

| 协议类型 | 特点 | 适用场景 |
|----------|------|----------|
| TCP | 可靠、面向连接、低延迟 | 原生客户端、高并发场景 |
| WebSocket | 全双工、基于HTTP、跨平台 | Web客户端、移动客户端 |

## 2. 基础协议结构

### 2.1 TCP协议结构

TCP协议采用固定长度的消息头+可变长度的消息体结构：

```
+-------------------+-------------------+-------------------+-------------------+
| 消息长度 (4字节)  | 消息类型 (2字节)  | 版本号 (1字节)    | 保留字段 (1字节)  |
+-------------------+-------------------+-------------------+-------------------+
|                          消息体 (可变长度)                               |
+-------------------------------------------------------------------------+
```

#### 2.1.1 字段说明

- **消息长度**：整个消息的总长度（包括消息头和消息体），使用网络字节序（大端序）
- **消息类型**：标识消息的类型（如登录请求、聊天消息等），使用网络字节序
- **版本号**：协议版本号，用于版本兼容
- **保留字段**：预留字段，用于未来扩展
- **消息体**：消息的具体内容，采用JSON格式

#### 2.1.2 消息解析流程

1. 读取4字节的消息长度
2. 根据消息长度读取剩余的消息内容
3. 解析消息头中的消息类型和版本号
4. 根据消息类型解析消息体

### 2.2 WebSocket协议结构

WebSocket协议使用标准的WebSocket帧结构，消息内容采用JSON格式：

```
+--------------+--------------+--------------+---------------------------+
| FIN (1位)    | RSV1-3 (3位) | 操作码 (4位) | 掩码 (1位) | 数据长度 (7位) |
+--------------+--------------+--------------+---------------------------+
| 扩展数据长度 (0/16/64位)                  |
+------------------------------------------+
| 掩码密钥 (0/4字节)                        |
+------------------------------------------+
| 数据载荷 (可变长度)                       |
+------------------------------------------+
```

#### 2.2.1 字段说明

- **FIN**：标识是否为消息的最后一个帧
- **RSV1-3**：预留字段，必须为0
- **操作码**：0x00表示延续帧，0x01表示文本帧，0x02表示二进制帧
- **掩码**：客户端发送的帧必须设置掩码位
- **数据长度**：数据载荷的长度，使用不同的编码方式表示0-2^63范围的长度
- **扩展数据长度**：当数据长度超过125时使用
- **掩码密钥**：用于解码数据载荷
- **数据载荷**：消息的具体内容，采用JSON格式

## 3. 消息类型定义

| 消息类型 | 代码 | 方向 | 描述 |
|----------|------|------|------|
| 登录请求 | 1001 | C→S | 客户端登录请求 |
| 登录响应 | 1002 | S→C | 服务器登录响应 |
| 注销请求 | 1003 | C→S | 客户端注销请求 |
| 注销响应 | 1004 | S→C | 服务器注销响应 |
| 单聊消息 | 2001 | C→S/C | 单聊消息 |
| 群聊消息 | 2002 | C→S/C | 群聊消息 |
| 消息确认 | 2003 | C→S | 消息接收/已读确认 |
| 用户状态更新 | 3001 | C→S/S→C | 用户在线状态更新 |
| 会话列表请求 | 3002 | C→S | 请求获取会话列表 |
| 会话列表响应 | 3003 | S→C | 返回会话列表 |
| 消息历史请求 | 3004 | C→S | 请求获取历史消息 |
| 消息历史响应 | 3005 | S→C | 返回历史消息 |
| 错误消息 | 9001 | S→C | 错误响应 |
| 心跳请求 | 9002 | C→S | 心跳包请求 |
| 心跳响应 | 9003 | S→C | 心跳包响应 |

## 4. 消息格式定义

### 4.1 HTTP登录接口

#### 4.1.1 HTTP登录请求

```
POST /api/login HTTP/1.1
Content-Type: application/json
Content-Length: 123

{
    "username": "testuser",
    "password": "password123",
    "client_info": {
        "platform": "android",
        "version": "1.0.0",
        "device_id": "device123"
    }
}
```

#### 4.1.2 HTTP登录响应

```
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 456

{
    "code": 0,
    "message": "success",
    "data": {
        "user_id": 1001,
        "username": "testuser",
        "nickname": "测试用户",
        "avatar_url": "http://example.com/avatar.jpg",
        "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMDAxIiwiZXhwIjoxNjI3ODQzMjAwfQ.SflKxwRJSMeKKF2QT4fwpMeJf36POk6yJV_adQssw5c",
        "expire_time": 1627843200,
        "tcp_endpoint": {
            "host": "192.168.1.100",
            "port": 8888
        }
    }
}
```

### 4.2 完整登录流程

```
客户端                                      服务器
  |                                          |
  | 1. HTTP登录请求                           |
  |----------------------------------------->|
  |                                          |
  |                                          | 验证用户名密码
  |                                          | 生成JWT token
  |                                          | 分配用户ID
  |                                          | 记录TCP端点
  |                                          |
  | 2. HTTP登录响应 (包含token和TCP端点)      |
  |<-----------------------------------------|
  |                                          |
  | 3. 建立TCP长连接                          |
  |==================TCP连接==================|
  |                                          |
  | 4. 发送Token认证消息 (1001)               |
  |----------------------------------------->|
  |                                          |
  |                                          | 验证token有效性
  |                                          | 解析用户ID
  |                                          | 绑定用户ID到TCP连接
  |                                          | 更新用户状态为在线
  |                                          |
  | 5. 认证成功响应 (1002)                    |
  |<-----------------------------------------|
  |                                          |
  | 6. 用户状态更新 (3001) - 在线             |
  |<-----------------------------------------|
```

### 4.3 TCP认证消息格式

#### 4.3.1 Token认证请求 (1001)

```json
{
    "auth_type": "token",
    "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMDAxIiwiZXhwIjoxNjI3ODQzMjAwfQ.SflKxwRJSMeKKF2QT4fwpMeJf36POk6yJV_adQssw5c",
    "client_info": {
        "platform": "android",
        "version": "1.0.0",
        "device_id": "device123"
    }
}
```

#### 4.3.2 认证成功响应 (1002)

```json
{
    "code": 0,
    "message": "Authentication successful",
    "data": {
        "user_id": 1001,
        "username": "testuser",
        "nickname": "测试用户",
        "session_key": "session_key_123456",
        "online_status": 1
    }
}
```

#### 4.3.3 认证失败响应 (1002)

```json
{
    "code": 1004,
    "message": "Invalid token",
    "data": {
        "user_id": null,
        "username": null,
        "nickname": null,
        "session_key": null,
        "online_status": 0
    }
}
```

### 4.4 用户ID与TCP连接绑定机制

#### 4.4.1 绑定数据结构

```cpp
struct UserConnection {
    uint32_t user_id;              // 用户ID
    std::string username;          // 用户名
    tcp::socket* socket;           // TCP连接socket指针
    std::chrono::steady_clock::time_point last_active; // 最后活跃时间
    uint8_t status;                // 在线状态：0-离线，1-在线，2-忙碌，3-离开
    std::string device_id;         // 设备ID
    std::string client_version;    // 客户端版本
};
```

#### 4.4.2 绑定流程

1. **Token验证**: 服务器验证JWT token的有效性
2. **用户识别**: 从token中提取用户ID和相关信息
3. **连接绑定**: 将用户ID与TCP socket建立映射关系
4. **状态更新**: 将用户状态更新为在线
5. **会话管理**: 为该连接分配会话密钥，用于后续消息加密

#### 4.4.3 消息路由机制

当服务器需要向特定用户发送消息时：

1. **查找连接**: 根据用户ID在连接映射表中查找对应的TCP连接
2. **验证状态**: 确认用户当前在线状态
3. **发送消息**: 通过找到的TCP连接发送消息
4. **失败处理**: 如果用户离线，消息将转入离线消息队列

```cpp
// 消息发送示例
bool sendMessageToUser(uint32_t user_id, uint16_t message_type, const std::string& message_body) {
    auto it = user_connections_.find(user_id);
    if (it == user_connections_.end()) {
        // 用户离线，存入离线消息队列
        saveOfflineMessage(user_id, message_type, message_body);
        return false;
    }
    
    if (it->second.status != 1) {  // 用户不在线
        saveOfflineMessage(user_id, message_type, message_body);
        return false;
    }
    
    // 发送消息到TCP连接
    return sendTcpMessage(*it->second.socket, message_type, message_body);
}
```

### 4.5 登录流程详细时序图

```mermaid
sequenceDiagram
    participant C as 客户端
    participant H as HTTP服务器
    participant T as TCP服务器
    participant DB as 数据库
    participant Cache as 缓存

    C->>H: 1. HTTP登录请求
    H->>DB: 2. 验证用户名密码
    DB-->>H: 3. 用户信息
    H->>Cache: 4. 生成JWT Token
    Cache-->>H: 5. Token
    H->>DB: 6. 记录登录信息
    H-->>C: 7. HTTP登录响应(Token+TCP端点)
    
    C->>T: 8. 建立TCP连接
    T-->>C: 9. TCP连接建立成功
    
    C->>T: 10. 发送Token认证消息
    T->>Cache: 11. 验证Token有效性
    Cache-->>T: 12. Token验证结果
    T->>DB: 13. 获取用户详细信息
    DB-->>T: 14. 用户信息
    T->>T: 15. 绑定用户ID到TCP连接
    T->>Cache: 16. 更新用户在线状态
    Cache-->>T: 17. 状态更新成功
    
    T-->>C: 18. 认证成功响应
    T->>C: 19. 用户状态更新(在线)
    
    Note over C,T: 登录完成，可以进行即时通讯
```

### 4.6 错误处理和重试机制

#### 4.6.1 HTTP登录失败

| 错误码 | HTTP状态码 | 描述 | 客户端处理 |
|--------|------------|------|------------|
| 1001 | 404 | 用户不存在 | 提示用户注册 |
| 1002 | 401 | 密码错误 | 提示重新输入密码 |
| 1003 | 409 | 用户已在线 | 强制下线或提示多端登录 |
| 5001 | 500 | 服务器内部错误 | 稍后重试 |

#### 4.6.2 TCP认证失败

| 错误码 | 描述 | 客户端处理 |
|--------|------|------------|
| 1004 | 无效的Token | 重新进行HTTP登录 |
| 1005 | Token已过期 | 重新进行HTTP登录 |
| 1006 | 连接已存在 | 关闭旧连接，建立新连接 |
| 5003 | TCP服务不可用 | 提示稍后重试 |

#### 4.6.3 重连机制

```cpp
// 客户端重连逻辑示例
class LoginManager {
public:
    void handleLoginFailure(int error_code) {
        switch (error_code) {
            case 1004: // Token无效
            case 1005: // Token过期
                // 重新进行HTTP登录
                retryHttpLogin();
                break;
            case 5003: // 服务不可用
                // 指数退避重试
                scheduleReconnectWithBackoff();
                break;
            default:
                // 其他错误，提示用户
                showErrorToUser(error_code);
                break;
        }
    }
    
private:
    void retryHttpLogin() {
        // 清除本地token，重新登录
        localStorage.remove("auth_token");
        performHttpLogin();
    }
    
    void scheduleReconnectWithBackoff() {
        int delay = std::min(30000, 1000 * std::pow(2, reconnect_attempts_));
        std::thread([this, delay]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            reconnect_attempts_++;
            establishTcpConnection();
        }).detach();
    }
};
```

### 4.7 单聊消息流程

```
客户端A                                     服务器                                     客户端B
  |                                          |                                          |
  | 单聊消息 (2001)                            |                                          |
  |----------------------------------------->|
  |                                          | 验证消息合法性
  |                                          | 存储消息
  |                                          |
  | 消息确认 (2003) - 已发送                   |                                          |
  |<-----------------------------------------|
  |                                          |
  |                                          | 单聊消息 (2001)
  |                                          |----------------------------------------->|
  |                                          |                                          |
  |                                          | 消息确认 (2003) - 已接收                   |
  |                                          |<-----------------------------------------|
  |                                          |
  | 消息确认 (2003) - 已接收                   |                                          |
  |<-----------------------------------------|
  |                                          |                                          |
  |                                          | 消息确认 (2003) - 已读                     |
  |                                          |<-----------------------------------------|
  |                                          |
  | 消息确认 (2003) - 已读                     |                                          |
  |<-----------------------------------------|
```

### 4.8 心跳流程

```
客户端                                      服务器
  |                                          |
  | 心跳请求 (9002)                            |
  |----------------------------------------->|
  |                                          |
  | 心跳响应 (9003)                            |
  |<-----------------------------------------|
```

### 4.9 离线消息流程

```
客户端A                                     服务器                                     客户端B
  |                                          |                                          |
  | 单聊消息 (2001)                            |                                          | (离线)
  |----------------------------------------->|
  |                                          | 验证消息合法性
  |                                          | 存储消息
  |                                          | 标记为未接收
  |                                          |
  | 消息确认 (2003) - 已发送                   |                                          |
  |<-----------------------------------------|
  |                                          |                                          |
  |                                          |                                          | 登录
  |                                          | 登录响应 (1002)                           |
  |                                          |<-----------------------------------------|
  |                                          |
  |                                          | 未读消息列表                              |
  |                                          |----------------------------------------->|
  |                                          |                                          |
  |                                          | 消息历史请求 (3004)                        |
  |                                          |<-----------------------------------------|
  |                                          |
  |                                          | 消息历史响应 (3005) - 包含离线消息         |
  |                                          |----------------------------------------->|
  |                                          |                                          |
  |                                          | 消息确认 (2003) - 已读                     |
  |                                          |<-----------------------------------------|
  |                                          |
  | 消息确认 (2003) - 已读                     |                                          |
  |<-----------------------------------------|
```

## 5. 错误码定义

| 错误码 | 消息 | 说明 |
|--------|------|------|
| 0 | success | 请求成功 |
| 1001 | User not found | 用户不存在 |
| 1002 | Invalid password | 密码错误 |
| 1003 | User already online | 用户已在线 |
| 1004 | Invalid token | 无效的令牌 |
| 1005 | Token expired | 令牌已过期 |
| 2001 | Session not found | 会话不存在 |
| 2002 | User not in group | 用户不在群聊中 |
| 2003 | Message too long | 消息过长 |
| 3001 | Permission denied | 权限不足 |
| 4001 | Invalid message format | 消息格式错误 |
| 4002 | Unsupported message type | 不支持的消息类型 |
| 4003 | Protocol version error | 协议版本错误 |
| 5001 | Server internal error | 服务器内部错误 |
| 5002 | Database error | 数据库错误 |
| 5003 | Service unavailable | 服务不可用 |

## 6. 安全性设计

### 6.1 数据传输安全

- **TCP协议**：建议使用TLS/SSL加密传输
- **WebSocket协议**：建议使用WSS（WebSocket Secure）加密传输

### 6.2 身份认证

- 使用JSON Web Token (JWT) 进行身份验证
- 令牌包含用户ID、过期时间等信息
- 令牌使用安全的算法（如HS256）签名

### 6.3 消息加密

- 敏感消息内容可进行端到端加密
- 服务器不存储加密消息的密钥

### 6.4 防止攻击

- 实现请求频率限制，防止DoS攻击
- 验证消息长度，防止缓冲区溢出
- 过滤恶意内容，防止XSS攻击
- 防止SQL注入攻击

## 7. 协议扩展

### 7.1 版本管理

协议支持版本管理，通过消息头中的版本号字段实现：

- 主版本号：当协议结构发生不兼容变化时增加
- 次版本号：当协议增加新功能但保持兼容时增加

### 7.2 扩展字段

消息体中可以包含`extra`字段，用于扩展消息内容：

```json
{
    "message_id": "msg123456",
    "session_id": "session123",
    "from_user_id": 1001,
    "to_user_id": 1002,
    "type": 0,
    "content": "你好",
    "timestamp": 1627843200,
    "extra": {
        "custom_field1": "value1",
        "custom_field2": "value2"
    }
}
```

## 8. 实现示例

### 8.1 TCP协议实现示例

```cpp
// 发送TCP消息
void sendTcpMessage(boost::asio::ip::tcp::socket& socket, uint16_t message_type, const std::string& json_body) {
    // 构建消息头
    uint32_t total_length = 8 + json_body.size(); // 4字节长度 + 4字节头 + 消息体
    uint16_t msg_type = htons(message_type);
    uint8_t version = 1;
    uint8_t reserved = 0;
    
    // 构建完整消息
    std::vector<char> message;
    message.resize(total_length);
    
    // 写入消息头
    *reinterpret_cast<uint32_t*>(&message[0]) = htonl(total_length);
    *reinterpret_cast<uint16_t*>(&message[4]) = msg_type;
    message[6] = version;
    message[7] = reserved;
    
    // 写入消息体
    std::memcpy(&message[8], json_body.c_str(), json_body.size());
    
    // 发送消息
    boost::asio::write(socket, boost::asio::buffer(message));
}

// 接收TCP消息
bool receiveTcpMessage(boost::asio::ip::tcp::socket& socket, uint16_t& out_message_type, std::string& out_json_body) {
    try {
        // 读取消息头
        char header[8];
        boost::asio::read(socket, boost::asio::buffer(header));
        
        // 解析消息头
        uint32_t total_length = ntohl(*reinterpret_cast<uint32_t*>(&header[0]));
        uint16_t message_type = ntohs(*reinterpret_cast<uint16_t*>(&header[4]));
        
        // 读取消息体
        uint32_t body_length = total_length - 8;
        std::vector<char> body(body_length);
        boost::asio::read(socket, boost::asio::buffer(body));
        
        out_message_type = message_type;
        out_json_body.assign(body.begin(), body.end());
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to receive TCP message: " << e.what());
        return false;
    }
}
```

### 8.2 WebSocket协议实现示例

```cpp
// 发送WebSocket消息
void sendWebSocketMessage(websocket::stream<tcp::socket>& ws, uint16_t message_type, const std::string& json_body) {
    // 创建完整的消息内容（包含消息类型和JSON体）
    std::string message = std::to_string(message_type) + "|" + json_body;
    
    // 发送WebSocket消息
    ws.write(boost::asio::buffer(message));
}

// 接收WebSocket消息
bool receiveWebSocketMessage(websocket::stream<tcp::socket>& ws, uint16_t& out_message_type, std::string& out_json_body) {
    try {
        // 接收WebSocket消息
        boost::beast::flat_buffer buffer;
        ws.read(buffer);
        
        // 解析消息
        std::string message = boost::beast::buffers_to_string(buffer.data());
        
        // 提取消息类型和JSON体（格式：type|json）
        size_t delimiter_pos = message.find('|');
        if (delimiter_pos == std::string::npos) {
            LOG_ERROR("Invalid WebSocket message format");
            return false;
        }
        
        std::string type_str = message.substr(0, delimiter_pos);
        uint16_t message_type = std::stoi(type_str);
        std::string json_body = message.substr(delimiter_pos + 1);
        
        out_message_type = message_type;
        out_json_body = json_body;
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to receive WebSocket message: " << e.what());
        return false;
    }
}
```

## 9. 总结

IMServer采用灵活、可扩展的协议设计，支持TCP和WebSocket两种通信方式，满足不同客户端的需求。协议结构清晰，消息类型丰富，支持完整的即时通讯功能。通过合理的通信流程设计和错误处理机制，确保了消息的可靠传输和处理。同时，协议设计考虑了安全性和扩展性，为未来功能扩展提供了良好的基础。