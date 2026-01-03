#pragma once

#include <vector>
#include <string>
#include <memory>

#include "network/ConnectionManager.h"

namespace protocol {

/**
 * @brief 消息对象抽象基类
 * 包含消息的完整信息，包括消息头和消息体
 */
class Message {
protected:
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
    Message(const std::vector<char>& body,
         network::ConnectionId connection_id = 0,
         network::ConnectionType connection_type = network::ConnectionType::TCP);
    virtual ~Message() = default;

    void bindConnection(network::ConnectionId connection_id, network::ConnectionType connection_type) {
        connection_id_ = connection_id;
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
};

/**
 * @brief TCP消息类
 */
class TCPMessage : public Message {
public:
    enum class DeserializeState {
        Header,  // 解析消息头
        Body     // 解析消息体
    };
    typedef struct {
        uint32_t total_length;  // 消息总长度
        uint16_t message_type;  // 消息类型
        uint8_t version;        // 协议版本
        uint8_t reserved;       // 保留字段
    } TcpMessageHeader;

private:
    DeserializeState state_;            // 当前反序列化状态
    TcpMessageHeader header_;           // TCP消息头
    std::vector<char> data_buffer_;     // 消息缓冲区，待处理数据
    size_t expected_body_length_;       // 预期的消息体长度

public:
    TCPMessage();
    TCPMessage(const std::vector<char>& body, network::ConnectionId connection_id = 0);

    void reset() override;

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

private:
    /**
     * @brief 解析消息头
     * @param consumed 已解析字节数引用
     * @return bool 是否完成解析头
     */
    bool deserializeHeader(size_t& consumed);
    
    /**
     * @brief 解析消息体
     * @param consumed 已解析字节数引用
     * @return bool 是否完成解析体
     */
    bool deserializeBody(size_t& consumed);
};

/**
 * @brief WebSocket消息类
 */
class WebSocketMessage : public Message {
public:
    enum class DeserializeState {
        Initial,       // 初始状态
        Header,        // 解析帧头
        PayloadLength, // 解析载荷长度
        ExtendedLength,// 解析扩展长度
        MaskingKey,    // 解析掩码密钥
        Payload,       // 解析载荷
        Complete       // 解析完成
    };

    struct WebSocketMessageHeader {
        uint8_t fin_opcode;  // FIN位和操作码
        uint8_t payload_len; // 载荷长度
        bool masked_;        // 是否掩码
        uint64_t extended_len; // 扩展长度
        std::vector<uint8_t> masking_key; // 掩码密钥
    };

    // 最高位掩码，用于判断是否为最终帧
    const uint8_t MASK_MASK = 0x80;
    // 基本长度掩码，用于获取基本长度
    const uint8_t BASIC_LENGTH_MASK = 0x7F;
    // 基本长度值，用于判断是否需要扩展长度
    const uint8_t BASIC_LENGTH = 126;
    // 帧头解析大小
    const uint8_t HEAD_PARSE_SIZE = 2;
    // 扩展长度解析大小
    const uint8_t EXTENDED_LENGTH_PARSE_SIZE = 8;
    // 掩码密钥解析大小
    const uint8_t MASKING_KEY_PARSE_SIZE = 4;

private:
    DeserializeState state_;  // 当前反序列化状态
    WebSocketMessageHeader header_;  // WebSocket消息头
    std::vector<char> data_buffer_;     // 消息缓冲区，待处理数据
    uint64_t expected_body_length_;          // 预期的消息体长度

public:
    WebSocketMessage();

    /**
     * @brief 构造函数，带初始化参数
     * @param message_type 消息类型
     * @param body 消息体
     * @param connection_id 连接ID
     * @param opcode WebSocket操作码
     * @param is_final 是否为最终帧
     */
    WebSocketMessage(const std::vector<char>& body, network::ConnectionId connection_id = 0);

    void reset() override;

    /**
     * @brief 获取WebSocket操作码
     * @return uint8_t 操作码
     */
    uint8_t getOpcode() const { return header_.fin_opcode & 0x0F; }
    
    /**
     * @brief 设置WebSocket操作码
     * @param opcode 操作码
     */
    void setOpcode(uint8_t opcode) { header_.fin_opcode = (header_.fin_opcode & 0xF0) | (opcode & 0x0F); }
    
    /**
     * @brief 获取是否为最终帧
     * @return bool 是否为最终帧
     */
    bool isFinal() const { return (header_.fin_opcode & MASK_MASK) != 0; }
    
    /**
     * @brief 设置是否为最终帧
     * @param is_final 是否为最终帧
     */
    void setIsFinal(bool is_final) { header_.fin_opcode = (header_.fin_opcode & 0x7F) | (is_final ? MASK_MASK : 0); }   
    
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

private:
    /**
     * @brief 解析帧头
     * @param consumed 已解析字节数引用
     * @return bool 是否完成解析头
     */
    bool deserializeHeader(size_t& consumed);
    
    /**
     * @brief 解析扩展长度
     * @param consumed 已解析字节数引用
     * @return bool 是否完成解析扩展长度
     */
    bool deserializeExtendedLength(size_t& consumed);
    
    /**
     * @brief 解析掩码密钥
     * @param consumed 已解析字节数引用
     * @return bool 是否完成解析掩码密钥
     */
    bool deserializeMaskingKey(size_t& consumed);
    
    /**
     * @brief 解析载荷
     * @param consumed 已解析字节数引用
     * @return bool 是否完成解析载荷
     */
    bool deserializePayload(size_t& consumed);

    /**
     * @brief 解析完成
     * @param consumed 已解析字节数引用
     * @return bool 是否完成解析
     */
    bool deserializeComplete(size_t& consumed);
};

/**
 * @brief HTTP消息类
 */
class HttpMessage : public Message {
public:
    enum class DeserializeState {
        Initial,              // 初始状态
        Headers,              // 解析头字段
        Body,                 // 解析消息体
        ChunkedBodyStart,     // 解析分块传输的块大小
        ChunkedBodyData,      // 解析分块数据
        ChunkedBodyEnd,       // 解析分块结束
        Complete              // 解析完成
    };

    struct HttpMessageHeader {
        std::string method_;        // 请求方法（GET, POST等）
        std::string url_;           // 请求URL
        std::string version_;       // HTTP版本（HTTP/1.1等）
        int status_code_;           // 响应状态码
        std::string status_message_; // 响应状态消息
        std::unordered_map<std::string, std::string> headers_; // 头字段
    };

    const std::string CRLF = "\r\n"; // 换行符

private:
    DeserializeState state_; // 当前解析状态
    bool is_parsing_; // 是否正在解析
    HttpMessageHeader header_; // HTTP消息头
    std::vector<char> data_buffer_;     // 消息缓冲区，待处理数据
    uint64_t expected_body_length_;          // 预期的消息体长度
    size_t current_chunk_size_;             // 当前分块大小

public:
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
    HttpMessage(const std::string& method, const std::string& url, const std::string& version,
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
    HttpMessage(const std::string& version, int status_code, const std::string& status_message,
                const std::unordered_map<std::string, std::string>& headers,
                const std::vector<char>& body,
                network::ConnectionId connection_id = 0);

    void reset() override;

    /**
     * @brief 获取请求方法
     * @return const std::string& 请求方法
     */
    const std::string& getMethod() const { return header_.method_; }
    
    /**
     * @brief 设置请求方法
     * @param method 请求方法
     */
    void setMethod(const std::string& method) { header_.method_ = method; }
    
    /**
     * @brief 获取请求URL
     * @return const std::string& 请求URL
     */
    const std::string& getUrl() const { return header_.url_; }
    
    /**
     * @brief 设置请求URL
     * @param url 请求URL
     */
    void setUrl(const std::string& url) { header_.url_ = url; }
    
    /**
     * @brief 获取HTTP版本
     * @return const std::string& HTTP版本
     */
    const std::string& getVersion() const { return header_.version_; }
    
    /**
     * @brief 设置HTTP版本
     * @param version HTTP版本
     */
    void setVersion(const std::string& version) { header_.version_ = version; }
    
    /**
     * @brief 获取响应状态码
     * @return int 响应状态码
     */
    int getStatusCode() const { return header_.status_code_; }
    
    /**
     * @brief 设置响应状态码
     * @param status_code 响应状态码
     */
    void setStatusCode(int status_code) { header_.status_code_ = status_code; }
    
    /**
     * @brief 获取响应状态消息
     * @return const std::string& 响应状态消息
     */
    const std::string& getStatusMessage() const { return header_.status_message_; }
    
    /**
     * @brief 设置响应状态消息
     * @param status_message 响应状态消息
     */
    void setStatusMessage(const std::string& status_message) { header_.status_message_ = status_message; }
    
    /**
     * @brief 获取头字段
     * @return const std::unordered_map<std::string, std::string>& 头字段
     */
    const std::unordered_map<std::string, std::string>& getHeaders() const { return header_.headers_; }
    
    /**
     * @brief 获取可修改的头字段
     * @return std::unordered_map<std::string, std::string>& 头字段
     */
    std::unordered_map<std::string, std::string>& getHeaders() { return header_.headers_; }
    
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

private:
    bool parseStartLine(const std::string& line);

    /**
     * @brief 反序列化HTTP消息起始行
     * @param consumed 已解析字节数
     * @return bool 是否完成解析
     */
    bool deserializeStartingLine(size_t& consumed);

    bool parseHeaderLine(size_t& consumed);
    
    /**
     * @brief 解析HTTP消息头
     * @param consumed 已解析字节数
     * @return bool 是否完成解析
     */
    bool deserializeHeaders(size_t& consumed);
    
    /**
     * @brief 解析HTTP消息体
     * @param consumed 已解析字节数
     * @return bool 是否完成解析
     */
    bool deserializeBody(size_t& consumed);
    
    /**
     * @brief 解析分块编码的HTTP消息体
     * @param consumed 已解析字节数
     * @return bool 是否完成解析
     */
    bool deserializeChunkedBodyStart(size_t& consumed);
    
    /**
     * @brief 解析分块编码的HTTP消息体数据
     * @param consumed 已解析字节数
     * @return bool 是否完成解析
     */
    bool deserializeChunkedBodyData(size_t& consumed);
    
    /**
     * @brief 解析分块编码的HTTP消息体结束
     * @param consumed 已解析字节数
     * @return bool 是否完成解析
     */
    bool deserializeChunkedBodyEnd(size_t& consumed);
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
    static std::shared_ptr<Message> deserialize(network::ConnectionType type, const std::vector<char>& data);
    
    /**
     * @brief 从字节流反序列化消息头
     * @param data 字节流数据
     * @param header 输出的消息头
     * @return bool 反序列化是否成功
     */
};

} // namespace protocol
