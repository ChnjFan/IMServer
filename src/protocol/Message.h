#pragma once

#include <vector>
#include <string>
#include <memory>

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
 * @brief 消息对象抽象基类
 * 包含消息的完整信息，包括消息头和消息体
 */
class Message {
protected:
    MessageHeader header_;                 // 消息头
    std::vector<char> body_;               // 消息体
    network::ConnectionId connection_id_;  // 关联的连接ID
    network::ConnectionType connection_type_; // 关联的连接类型

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
     * @brief 虚析构函数
     */
    virtual ~Message() = default;
    
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
    virtual std::vector<char> serialize() const = 0;
    
    /**
     * @brief 从字节流解析消息
     * @param data 字节流数据
     * @return bool 解析是否成功
     */
    virtual bool deserialize(const std::vector<char>& data) = 0;
    
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
};

/**
 * @brief TCP消息类
 */
class TCPMessage : public Message {
public:
    /**
     * @brief 构造函数
     */
    TCPMessage();
    
    /**
     * @brief 构造函数，带初始化参数
     * @param message_type 消息类型
     * @param body 消息体
     * @param connection_id 连接ID
     */
    TCPMessage(
        uint16_t message_type,
        const std::vector<char>& body,
        network::ConnectionId connection_id = 0);
    
    /**
     * @brief 将消息转换为字节流
     * @return std::vector<char> 序列化后的字节流
     */
    std::vector<char> serialize() const override;
    
    /**
     * @brief 从字节流解析消息
     * @param data 字节流数据
     * @return bool 解析是否成功
     */
    bool deserialize(const std::vector<char>& data) override;
};

/**
 * @brief WebSocket消息类
 */
class WebSocketMessage : public Message {
private:
    uint8_t opcode_;  // WebSocket操作码
    bool is_final_;   // 是否为最终帧

public:
    /**
     * @brief 构造函数
     */
    WebSocketMessage();
    
    /**
     * @brief 构造函数，带初始化参数
     * @param message_type 消息类型
     * @param body 消息体
     * @param connection_id 连接ID
     * @param opcode WebSocket操作码
     * @param is_final 是否为最终帧
     */
    WebSocketMessage(
        uint16_t message_type,
        const std::vector<char>& body,
        network::ConnectionId connection_id = 0,
        uint8_t opcode = 0x01, // 默认文本帧
        bool is_final = true);
    
    /**
     * @brief 获取WebSocket操作码
     * @return uint8_t 操作码
     */
    uint8_t getOpcode() const { return opcode_; }
    
    /**
     * @brief 设置WebSocket操作码
     * @param opcode 操作码
     */
    void setOpcode(uint8_t opcode) { opcode_ = opcode; }
    
    /**
     * @brief 获取是否为最终帧
     * @return bool 是否为最终帧
     */
    bool isFinal() const { return is_final_; }
    
    /**
     * @brief 设置是否为最终帧
     * @param is_final 是否为最终帧
     */
    void setIsFinal(bool is_final) { is_final_ = is_final; }
    
    /**
     * @brief 将消息转换为字节流
     * @return std::vector<char> 序列化后的字节流
     */
    std::vector<char> serialize() const override;
    
    /**
     * @brief 从字节流解析消息
     * @param data 字节流数据
     * @return bool 解析是否成功
     */
    bool deserialize(const std::vector<char>& data) override;
};

/**
 * @brief HTTP消息类
 */
class HttpMessage : public Message {
private:
    std::string method_;        // 请求方法（GET, POST等）
    std::string url_;           // 请求URL
    std::string version_;       // HTTP版本（HTTP/1.1等）
    int status_code_;           // 响应状态码
    std::string status_message_; // 响应状态消息
    std::unordered_map<std::string, std::string> headers_; // 头字段

public:
    /**
     * @brief 构造函数
     */
    HttpMessage();
    
    /**
     * @brief 构造函数，带初始化参数（请求）
     * @param method 请求方法
     * @param url 请求URL
     * @param version HTTP版本
     * @param headers 头字段
     * @param body 消息体
     * @param connection_id 连接ID
     */
    HttpMessage(
        const std::string& method,
        const std::string& url,
        const std::string& version,
        const std::unordered_map<std::string, std::string>& headers,
        const std::vector<char>& body,
        network::ConnectionId connection_id = 0);
    
    /**
     * @brief 构造函数，带初始化参数（响应）
     * @param version HTTP版本
     * @param status_code 响应状态码
     * @param status_message 响应状态消息
     * @param headers 头字段
     * @param body 消息体
     * @param connection_id 连接ID
     */
    HttpMessage(
        const std::string& version,
        int status_code,
        const std::string& status_message,
        const std::unordered_map<std::string, std::string>& headers,
        const std::vector<char>& body,
        network::ConnectionId connection_id = 0);
    
    /**
     * @brief 获取请求方法
     * @return const std::string& 请求方法
     */
    const std::string& getMethod() const { return method_; }
    
    /**
     * @brief 设置请求方法
     * @param method 请求方法
     */
    void setMethod(const std::string& method) { method_ = method; }
    
    /**
     * @brief 获取请求URL
     * @return const std::string& 请求URL
     */
    const std::string& getUrl() const { return url_; }
    
    /**
     * @brief 设置请求URL
     * @param url 请求URL
     */
    void setUrl(const std::string& url) { url_ = url; }
    
    /**
     * @brief 获取HTTP版本
     * @return const std::string& HTTP版本
     */
    const std::string& getVersion() const { return version_; }
    
    /**
     * @brief 设置HTTP版本
     * @param version HTTP版本
     */
    void setVersion(const std::string& version) { version_ = version; }
    
    /**
     * @brief 获取响应状态码
     * @return int 响应状态码
     */
    int getStatusCode() const { return status_code_; }
    
    /**
     * @brief 设置响应状态码
     * @param status_code 响应状态码
     */
    void setStatusCode(int status_code) { status_code_ = status_code; }
    
    /**
     * @brief 获取响应状态消息
     * @return const std::string& 响应状态消息
     */
    const std::string& getStatusMessage() const { return status_message_; }
    
    /**
     * @brief 设置响应状态消息
     * @param status_message 响应状态消息
     */
    void setStatusMessage(const std::string& status_message) { status_message_ = status_message; }
    
    /**
     * @brief 获取头字段
     * @return const std::unordered_map<std::string, std::string>& 头字段
     */
    const std::unordered_map<std::string, std::string>& getHeaders() const { return headers_; }
    
    /**
     * @brief 获取可修改的头字段
     * @return std::unordered_map<std::string, std::string>& 头字段
     */
    std::unordered_map<std::string, std::string>& getHeaders() { return headers_; }
    
    /**
     * @brief 将消息转换为字节流
     * @return std::vector<char> 序列化后的字节流
     */
    std::vector<char> serialize() const override;
    
    /**
     * @brief 从字节流解析消息
     * @param data 字节流数据
     * @return bool 解析是否成功
     */
    bool deserialize(const std::vector<char>& data) override;
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
     * @return std::shared_ptr<Message> 反序列化后的消息对象
     */
    static std::shared_ptr<Message> deserialize(const std::vector<char>& data);
    
    /**
     * @brief 从字节流反序列化消息头
     * @param data 字节流数据
     * @param header 输出的消息头
     * @return bool 反序列化是否成功
     */
    static bool deserializeHeader(const std::vector<char>& data, MessageHeader& header);
};

} // namespace protocol
