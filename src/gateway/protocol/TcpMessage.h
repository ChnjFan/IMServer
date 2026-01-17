#pragma once

#include "Message.h"

namespace protocol {

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

    /**
     * @brief 获取消息类型
     * @return network::ConnectionType 消息类型
     */
    network::ConnectionType getConnectionType() const override { return network::ConnectionType::TCP; }

    MessageType getMessageType() const override { return header_.message_type; }

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

} // namespace protocol