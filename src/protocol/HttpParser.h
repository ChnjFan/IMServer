#pragma once

#include "Parser.h"

namespace protocol {

// HTTP解析状态枚举
enum class HttpParseState {
    Initial,              // 初始状态
    Headers,              // 解析头字段
    Body,                 // 解析消息体
    ChunkedBodyStart,     // 解析分块传输的块大小
    ChunkedBodyData,      // 解析分块数据
    ChunkedBodyEnd,       // 解析分块结束
    Complete              // 解析完成
};

/**
 * @brief HTTP协议解析器
 * 解析HTTP请求，生成统一的Message对象
 */
class HttpParser : public Parser {
private:
    /**
     * @brief 解析HTTP请求行或响应行
     * @param start_line 请求行或响应行
     * @param message HTTP消息对象
     * @return bool 解析是否成功
     */
    bool parseStartLine(const std::string& start_line, ::protocol::HttpMessage& message);
    
    /**
     * @brief 解析HTTP头字段
     * @param buffer 待解析的数据
     * @param consumed 已消费的数据长度
     * @param message HTTP消息对象
     * @return bool 头字段是否解析完成
     */
    bool parseHeaders(const std::vector<char>& buffer, size_t& consumed, ::protocol::HttpMessage& message);
    
    // HTTP解析相关
    std::vector<char> buffer_;          // 解析缓冲区
    HttpParseState state_;              // 当前解析状态
    size_t content_length_;             // 消息体长度
    bool chunked_;                      // 是否为分块传输
    size_t current_chunk_size_;         // 当前分块大小
    std::unique_ptr<::protocol::HttpMessage> message_; // 当前解析的消息
    std::mutex mutex_;                 // 互斥锁，保护解析状态

public:
    /**
     * @brief 构造函数
     */
    HttpParser();
    
    /**
     * @brief 析构函数
     */
    ~HttpParser() override = default;
    
    /**
     * @brief 异步解析HTTP数据
     * @param data 待解析的原始数据
     * @param callback 解析完成回调函数
     */
    void asyncParse(
        const std::vector<char>& data,
        ParseCallback callback) override;
    
    /**
     * @brief 获取协议类型
     * @return network::ConnectionType::HTTP
     */
    network::ConnectionType getType() const override {
        return network::ConnectionType::HTTP;
    }
    
    /**
     * @brief 重置解析器状态
     */
    void reset() override;
};

} // namespace protocol
