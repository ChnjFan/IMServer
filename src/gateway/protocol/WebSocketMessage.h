#pragma once

#include "Message.h"

namespace protocol {

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
    
    /**
     * @brief 获取消息类型
     * @return network::ConnectionType 消息类型
     */
    network::ConnectionType getConnectionType() const override { return network::ConnectionType::WebSocket; }

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
    bool deserializeComplete();
};

} // namespace protocol