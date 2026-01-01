#pragma once

#include <vector>
#include <string>

#include "network/ConnectionManager.h"

// 消息头结构
typedef struct {
    uint32_t total_length;  // 消息总长度
    uint16_t message_type;  // 消息类型
    uint8_t version;        // 协议版本
    uint8_t reserved;       // 保留字段
} MessageHeader;

namespace protocol {

/**
 * @brief 消息对象类
 * 包含消息的完整信息，包括消息头和消息体
 */
class Message {
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
    Message(
        uint16_t message_type,
        const std::vector<char>& body,
        network::ConnectionId connection_id = 0,
        network::ConnectionType connection_type = network::ConnectionType::TCP);
    
    /**
     * @brief 获取消息头
     * @return const MessageHeader& 消息头引用
     */
    const MessageHeader& getHeader() const { return header_; }
    
    /**
     * @brief 获取可修改的消息头
     * @return MessageHeader& 消息头引用
     */
    MessageHeader& getHeader() { return header_; }
    
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
    
    /**
     * @brief 获取消息类型
     * @return uint16_t 消息类型
     */
    uint16_t getMessageType() const { return header_.message_type; }
    
    /**
     * @brief 设置消息类型
     * @param message_type 消息类型
     */
    void setMessageType(uint16_t message_type) {
        header_.message_type = message_type;
    }
    
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
     * @brief 获取连接类型
     * @return network::ConnectionType 连接类型
     */
    network::ConnectionType getConnectionType() const { return connection_type_; }
    
    /**
     * @brief 设置连接类型
     * @param connection_type 连接类型
     */
    void setConnectionType(network::ConnectionType connection_type) {
        connection_type_ = connection_type;
    }
    
    /**
     * @brief 获取消息总长度
     * @return uint32_t 消息总长度
     */
    uint32_t getTotalLength() const { return header_.total_length; }
    
    /**
     * @brief 设置消息总长度
     * @param total_length 消息总长度
     */
    void setTotalLength(uint32_t total_length) {
        header_.total_length = total_length;
    }
    
    /**
     * @brief 解析JSON消息体
     * @tparam T 解析目标类型
     * @return T 解析结果
     */
    template<typename T>
    T parseJson() const;
    
    /**
     * @brief 序列化JSON到消息体
     * @tparam T 序列化源类型
     * @param data 要序列化的数据
     */
    template<typename T>
    void serializeJson(const T& data);
    
    /**
     * @brief 将消息转换为字节流
     * @return std::vector<char> 序列化后的字节流
     */
    std::vector<char> serialize() const;
    
    /**
     * @brief 从字节流解析消息
     * @param data 字节流数据
     * @return bool 解析是否成功
     */
    bool deserialize(const std::vector<char>& data);
    
    /**
     * @brief 获取协议版本
     * @return uint8_t 协议版本
     */
    uint8_t getVersion() const { return header_.version; }
    
    /**
     * @brief 设置协议版本
     * @param version 协议版本
     */
    void setVersion(uint8_t version) {
        header_.version = version;
    }
    
    /**
     * @brief 计算并更新消息总长度
     */
    void updateTotalLength();
    
private:
    MessageHeader header_;                 ///< 消息头
    std::vector<char> body_;               ///< 消息体
    network::ConnectionId connection_id_;  ///< 关联的连接ID
    network::ConnectionType connection_type_; ///< 关联的连接类型
};

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
     * @param message 输出的消息对象
     * @return bool 反序列化是否成功
     */
    static bool deserialize(const std::vector<char>& data, Message& message);
    
    /**
     * @brief 从字节流反序列化消息头
     * @param data 字节流数据
     * @param header 输出的消息头
     * @return bool 反序列化是否成功
     */
    static bool deserializeHeader(const std::vector<char>& data, MessageHeader& header);
};

} // namespace protocol
