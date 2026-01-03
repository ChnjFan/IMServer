#pragma once

#include "Parser.h"

namespace protocol {

/**
 * @brief WebSocket协议解析器
 * 解析WebSocket帧，提取实际消息内容
 */
class WebSocketParser : public Parser {
private:
    bool is_parsing_;                   // 是否正在解析中
    WebSocketMessage parsed_message_;   // 当前正在解析的消息
    std::mutex mutex_;                  // 互斥锁，保护解析状态

public:
    WebSocketParser();
    ~WebSocketParser() override = default;

    void reset() override;

    /**
     * @brief 异步解析WebSocket数据
     * @param data 待解析的原始数据
     * @param callback 解析完成回调函数
     */
    void asyncParse(const std::vector<char>& data, ParseCallback callback) override;

    /**
     * @brief 获取协议类型
     * @return network::ConnectionType::WebSocket
     */
    network::ConnectionType getType() const override {
        return network::ConnectionType::WebSocket;
    }
};

} // namespace protocol
