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

### 4.1 登录请求 (1001)

```json
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

### 4.2 登录响应 (1002)

```json
{
    "code": 0,
    "message": "success",
    "data": {
        "user_id": 1001,
        "username": "testuser",
        "nickname": "测试用户",
        "avatar_url": "http://example.com/avatar.jpg",
        "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
        "expire_time": 1627843200
    }
}
```

### 4.3 单聊消息 (2001)

```json
{
    "message_id": "msg123456",
    "session_id": "session123",
    "from_user_id": 1001,
    "to_user_id": 1002,
    "type": 0, // 0: 文本, 1: 图片, 2: 语音, 3: 视频, 4: 文件
    "content": "你好，这是一条测试消息",
    "timestamp": 1627843200,
    "extra": {
        "push": true
    }
}
```

### 4.4 群聊消息 (2002)

```json
{
    "message_id": "msg123457",
    "session_id": "session124",
    "group_id": 2001,
    "from_user_id": 1001,
    "type": 0,
    "content": "大家好，这是一条群消息",
    "timestamp": 1627843300,
    "extra": {
        "mention_users": [1002, 1003]
    }
}
```

### 4.5 消息确认 (2003)

```json
{
    "message_id": "msg123456",
    "status": 2, // 1: 已发送, 2: 已接收, 3: 已读
    "timestamp": 1627843250
}
```

### 4.6 用户状态更新 (3001)

```json
{
    "user_id": 1001,
    "status": 1, // 0: 离线, 1: 在线, 2: 忙碌, 3: 离开
    "timestamp": 1627843200
}
```

### 4.7 会话列表请求 (3002)

```json
{
    "page": 1,
    "page_size": 20
}
```

### 4.8 会话列表响应 (3003)

```json
{
    "code": 0,
    "message": "success",
    "data": {
        "total": 10,
        "page": 1,
        "page_size": 20,
        "sessions": [
            {
                "session_id": "session123",
                "type": 0, // 0: 单聊, 1: 群聊
                "user_id": 1002,
                "nickname": "好友1",
                "avatar_url": "http://example.com/avatar1.jpg",
                "last_message": "你好",
                "last_message_time": 1627843200,
                "unread_count": 2
            },
            {
                "session_id": "session124",
                "type": 1,
                "group_id": 2001,
                "group_name": "测试群",
                "avatar_url": "http://example.com/group1.jpg",
                "last_message": "大家好",
                "last_message_time": 1627843300,
                "unread_count": 1
            }
        ]
    }
}
```

### 4.9 消息历史请求 (3004)

```json
{
    "session_id": "session123",
    "last_message_id": "msg123450",
    "count": 20
}
```

### 4.10 消息历史响应 (3005)

```json
{
    "code": 0,
    "message": "success",
    "data": {
        "session_id": "session123",
        "messages": [
            {
                "message_id": "msg123441",
                "from_user_id": 1002,
                "to_user_id": 1001,
                "type": 0,
                "content": "你好吗？",
                "timestamp": 1627843000,
                "status": 3 // 已读
            },
            {
                "message_id": "msg123442",
                "from_user_id": 1001,
                "to_user_id": 1002,
                "type": 0,
                "content": "我很好，谢谢！",
                "timestamp": 1627843100,
                "status": 3
            }
        ]
    }
}
```

### 4.11 错误消息 (9001)

```json
{
    "code": 1001,
    "message": "User not found",
    "timestamp": 1627843200
}
```

### 4.12 心跳请求 (9002)

```json
{
    "timestamp": 1627843200
}
```

### 4.13 心跳响应 (9003)

```json
{
    "timestamp": 1627843200
}
```

## 4. 通信流程

### 4.1 登录流程

```
客户端                                      服务器
  |                                          |
  | 登录请求 (1001)                           |
  |----------------------------------------->|
  |                                          |
  |                                          | 验证用户身份
  |                                          |
  |                                          | 生成令牌
  |                                          |
  | 登录响应 (1002)                           |
  |<-----------------------------------------|
  |                                          |
  | 用户状态更新 (3001) - 在线                 |
  |<-----------------------------------------|
```

### 4.2 单聊消息流程

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

### 4.3 心跳流程

```
客户端                                      服务器
  |                                          |
  | 心跳请求 (9002)                            |
  |----------------------------------------->|
  |                                          |
  | 心跳响应 (9003)                            |
  |<-----------------------------------------|
```

### 4.4 离线消息流程

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