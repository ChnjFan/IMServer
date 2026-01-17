#pragma once

#include "Parser.h"
#include "TcpMessage.h"

namespace protocol {

/**
 * @brief TCP协议解析器
 * 解析固定长度消息头+可变长度消息体的TCP协议格式
 */
class TcpParser : public Parser {
private:
    bool is_parsing_;                 // 是否正在解析中
    TCPMessage parsed_message_;       // 已解析的消息
    std::mutex mutex_;                 // 互斥锁，保护解析状态

public:
    TcpParser();
    ~TcpParser() override = default;

    /**
     * @brief 重置解析器状态
     */
    void reset() override;

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
};

} // namespace protocol
