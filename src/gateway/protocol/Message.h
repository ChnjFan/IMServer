#pragma once

#include <vector>
#include <string>
#include <memory>

#include "ConnectionManager.h"

namespace protocol {

/**
 * @brief 消息对象抽象基类
 * 包含消息的完整信息，包括消息头和消息体
 */
class Message {
public:
    enum class MessageType : int32_t {
        LoginRequest = 1001,
        LoginResponse = 1002,
        LogoutRequest = 1003,
        LogoutResponse = 1004,
        RegisterRequest = 1005,
        RegisterResponse = 1006,
        ChatRequest = 2001,
        GroupChatRequest = 2002,
        MessageAck = 2003,
        UserStatusUpdate = 3001,
        SessionListRequest = 3002,
        SessionListResponse = 3003,
        MessageHistoryRequest = 3004,
        MessageHistoryResponse = 3005,
        ErrorResponse = 9001,
        HeartbeatRequest = 9002,
        HeartbeatResponse = 9003,
    };

protected:
    std::vector<char> body_;               // 消息体
    network::ConnectionId connection_id_;  // 关联的连接ID
    network::ConnectionType connection_type_; // 关联的连接类型
    std::string message_id_;                // 消息ID

public:
    /**
     * @brief 构造函数
     */
    Message();
    
    /**
     * @brief 构造函数，带初始化参数
     * @param message_type 消息类型
     * @param body 消息体
     * @param connection_id 连接ID
     * @param connection_type 连接类型
     */
    Message(const std::vector<char>& body,
         network::ConnectionId connection_id = 0,
         network::ConnectionType connection_type = network::ConnectionType::TCP);
    virtual ~Message() = default;

    void bindConnection(network::ConnectionId connection_id, network::ConnectionType connection_type) {
        connection_id_ = connection_id;
        // 绑定连接类型时，将连接ID添加到消息ID末尾，用于区分不同连接类型的消息
        // 例如：messid_TCP_1234567890
        message_id_ += "_" + messageConnectionTypeToString(connection_type);
        message_id_ += "_" + std::to_string(connection_id);
        connection_type_ = connection_type;
    }

    /**
     * @brief 获取消息体
     * @return const std::vector<char>& 消息体引用
     */
    const std::vector<char>& getBody() const { return body_; }

    /**
     * @brief 获取可修改的消息体
     * @return std::vector<char>& 消息体引用
     */
    std::vector<char>& getBody() { return body_; }
    std::string getPayload() const { return std::string(body_.begin(), body_.end()); }

    /**
     * @brief 获取连接ID
     * @return network::ConnectionId 连接ID
     */
    network::ConnectionId getConnectionId() const { return connection_id_; }
    
    /**
     * @brief 设置连接ID
     * @param connection_id 连接ID
     */
    void setConnectionId(network::ConnectionId connection_id) {
        connection_id_ = connection_id;
    }
    
    /**
     * @brief 设置连接类型
     * @param connection_type 连接类型
     */
    void setConnectionType(network::ConnectionType connection_type) {
        connection_type_ = connection_type;
    }

    /**
     * @brief 将消息类型转换为字符串
     * @param type 消息类型
     * @return std::string 消息类型的字符串表示
     */
    static std::string messageConnectionTypeToString(network::ConnectionType type);

    /**
     * @brief 重置消息状态
     */
    virtual void reset() = 0;

    /**
     * @brief 将消息转换为字节流
     * @return std::vector<char> 序列化后的字节流
     */
    virtual std::vector<char> serialize() const = 0;

    /**
     * @brief 从字节流解析消息
     * @param data 字节流数据
     * @return bool 解析是否成功
     */
    virtual bool deserialize(const std::vector<char>& data) = 0;

    /**
     * @brief 获取消息类型
     * @return network::ConnectionType 消息类型
     */
    virtual network::ConnectionType getConnectionType() const = 0;

    /**
     * @brief 获取消息类型
     * @return MessageType 消息类型
     */
    virtual MessageType getMessageType() const = 0;

    /**
     * @brief 获取消息ID
     * @return std::string 消息ID
     */
    std::string getMessageId() const { return message_id_; }
};

// 前向声明
class TCPMessage;
class WebSocketMessage;
class HttpMessage;

/**
 * @brief 消息序列化工具类
 */
class MessageSerializer {
public:
    /**
     * @brief 序列化消息到字节流
     * @param message 消息对象
     * @return std::vector<char> 序列化后的字节流
     */
    static std::vector<char> serialize(const Message& message);
    
    /**
     * @brief 从字节流反序列化消息
     * @param data 字节流数据
     * @return std::shared_ptr<Message> 反序列化后的消息对象
     */
    static std::shared_ptr<Message> deserialize(network::ConnectionType type, const std::vector<char>& data);
    
    /**
     * @brief 从字节流反序列化消息头
     * @param data 字节流数据
     * @param header 输出的消息头
     * @return bool 反序列化是否成功
     */
};

} // namespace protocol
