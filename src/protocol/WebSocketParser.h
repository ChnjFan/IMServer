#pragma once

#include "Parser.h"

namespace protocol {

/**
 * @brief WebSocket协议解析器
 * 解析WebSocket帧，提取实际消息内容
 */
class WebSocketParser : public Parser {
private:
    /**
     * @brief WebSocket帧解析状态枚举
     */
    enum class FrameState {
        Initial,       // 初始状态
        Header,        // 解析帧头
        PayloadLength, // 解析载荷长度
        ExtendedLength,// 解析扩展长度
        MaskingKey,    // 解析掩码密钥
        Payload,       // 解析载荷
        Complete       // 解析完成
    };

    const uint8_t FIN_MASK = 0x80;
    const uint8_t MASK_MASK = 0x80;
    const uint8_t RSV1_MASK = 0x40;
    const uint8_t RSV2_MASK = 0x20;
    const uint8_t RSV3_MASK = 0x10;
    const uint8_t OPCODE_MASK = 0x0F;
    const uint8_t BASIC_LENGTH_MASK = 0x7F;
    const uint8_t BASIC_LENGTH = 126;
    const uint8_t HEAD_PARSE_SIZE = 2;
    const uint8_t EXTENDED_LENGTH_PARSE_SIZE = 8;
    const uint8_t MASKING_KEY_PARSE_SIZE = 4;

    // WebSocket帧相关
    FrameState state_;                 // 当前解析状态
    std::vector<char> current_frame_;  // 当前帧数据
    std::vector<char> message_buffer_; // 完整消息缓冲区
    bool is_final_frame_;              // 是否为消息的最后一帧
    uint8_t opcode_;                   // 当前帧操作码
    bool masked_;                      // 是否为掩码帧
    uint64_t payload_length_;          // 载荷长度
    std::vector<char> masking_key_;    // 掩码密钥
    std::mutex mutex_;                 // 互斥锁，保护解析状态

public:
    WebSocketParser();
    ~WebSocketParser() override = default;

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

    /**
     * @brief 重置解析器状态
     */
    void reset() override;

private:
    /**
     * @brief 解析WebSocket帧头
     * @param data 待解析的数据
     * @param consumed 已消费的数据长度
     * @return bool 消息头是否解析成功
     */
    bool parseHeader(const std::vector<char>& data, size_t& consumed);
    bool parseExtendedLength(const std::vector<char>& data, size_t& consumed);
    bool parseMaskingKey(const std::vector<char>& data, size_t& consumed);
    bool parsePayload(const std::vector<char>& data, size_t& consumed);
    bool parseComplete(WebSocketMessage& message);
};

} // namespace protocol
