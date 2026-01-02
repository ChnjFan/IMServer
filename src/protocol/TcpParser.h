#pragma once

#include "Parser.h"

namespace protocol {

/**
 * @brief TCP协议解析器
 * 解析固定长度消息头+可变长度消息体的TCP协议格式
 */
class TcpParser : public Parser {
private:
    /**
     * @brief 解析状态枚举
     */
    enum class ParseState {
        Header,  // 解析消息头
        Body     // 解析消息体
    };

    // 状态机相关
    ParseState state_;                 // 当前解析状态
    MessageHeader header_;             // 当前解析的消息头
    std::vector<char> body_buffer_;    // 消息体缓冲区
    size_t expected_body_length_;      // 预期的消息体长度
    std::mutex mutex_;                 // 互斥锁，保护解析状态
public:
    TcpParser();
    ~TcpParser() override = default;

    /**
     * @brief 异步解析TCP数据
     * @param data 待解析的原始数据
     * @param callback 解析完成回调函数
     */
    void asyncParse(const std::vector<char>& data, ParseCallback callback) override;

    /**
     * @brief 获取协议类型
     * @return network::ConnectionType::TCP
     */
    network::ConnectionType getType() const override {
        return network::ConnectionType::TCP;
    }

    /**
     * @brief 重置解析器状态
     */
    void reset() override;
    
private:    
    /**
     * @brief 解析消息头
     * @param data 待解析的数据
     * @param consumed 已消费的数据长度
     * @return bool 解析是否成功
     */
    bool parseHeader(const std::vector<char>& data, size_t& consumed);
    
    /**
     * @brief 解析消息体
     * @param data 待解析的数据
     * @param consumed 已消费的数据长度
     * @return bool 解析是否成功
     */
    bool parseBody(const std::vector<char>& data, size_t& consumed);
};

} // namespace protocol
