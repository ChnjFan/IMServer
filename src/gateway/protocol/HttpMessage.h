#pragma once

#include <unordered_map>
#include <string>
#include "Message.h"

namespace protocol {

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
    MessageType message_type_;          // 消息类型
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

    /**
     * @brief 获取消息类型
     * @return network::ConnectionType 消息类型
     */
    network::ConnectionType getConnectionType() const override { return network::ConnectionType::HTTP; }

    /**
     * @brief 获取消息类型
     * @return MessageType 消息类型
     */
    MessageType getMessageType() const override { return message_type_; }

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

} // namespace protocol