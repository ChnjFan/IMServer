#include "WebSocketMessage.h"

#include <cstring>
#include <algorithm>

// 字节序转换函数所需的头文件
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

namespace protocol {

// ---------------------- WebSocketMessage子类实现 ----------------------

WebSocketMessage::WebSocketMessage() : Message() {
    reset();
}

WebSocketMessage::WebSocketMessage(const std::vector<char>& body, network::ConnectionId connection_id) : 
    Message(body, connection_id, network::ConnectionType::WebSocket) {
    reset();
}

void WebSocketMessage::reset()
{
    body_.clear();
    connection_id_ = 0;
    connection_type_ = network::ConnectionType::WebSocket;
    state_ = DeserializeState::Initial;
    data_buffer_.clear();
    expected_body_length_ = 0;
}

std::vector<char> WebSocketMessage::serialize() const {
    // WebSocket消息的序列化需要遵循WebSocket协议帧格式
    // 这里简化实现，只处理基本的文本帧
    std::vector<char> buffer;
    
    // 第1字节：FIN + RSV1 + RSV2 + RSV3 + OPCODE
    uint8_t first_byte = (isFinal() ? 0x80 : 0x00) | getOpcode();
    buffer.push_back(static_cast<char>(first_byte));
    
    // 第2字节：MASK + PAYLOAD LENGTH
    uint8_t second_byte = 0x00; // 服务器发送的消息不使用掩码
    size_t payload_length = body_.size();
    
    if (payload_length <= 125) {
        second_byte |= static_cast<uint8_t>(payload_length);
        buffer.push_back(static_cast<char>(second_byte));
    } else if (payload_length <= 65535) {
        second_byte |= 126;
        buffer.push_back(static_cast<char>(second_byte));
        // 添加16位长度
        uint16_t length16 = htons(static_cast<uint16_t>(payload_length));
        buffer.insert(buffer.end(), reinterpret_cast<const char*>(&length16), 
                      reinterpret_cast<const char*>(&length16) + sizeof(uint16_t));
    } else {
        second_byte |= 127;
        buffer.push_back(static_cast<char>(second_byte));
        // 添加64位长度
        uint64_t length64 = htobe64(static_cast<uint64_t>(payload_length));
        buffer.insert(buffer.end(), reinterpret_cast<const char*>(&length64), 
                      reinterpret_cast<const char*>(&length64) + sizeof(uint64_t));
    }
    
    // 添加消息体
    buffer.insert(buffer.end(), body_.begin(), body_.end());
    
    return buffer;
}

bool WebSocketMessage::deserialize(const std::vector<char>& data) {
    bool message_complete = false;
    size_t consumed = 0;

    data_buffer_.insert(data_buffer_.end(), data.begin(), data.end());

    while (consumed < data_buffer_.size() && !message_complete) {
        switch (state_) {
            case DeserializeState::Initial:
                expected_body_length_ = 0;
                state_ = DeserializeState::Header;
                break;
            case DeserializeState::Header:
                message_complete = deserializeHeader(consumed);
                break;
            case DeserializeState::ExtendedLength:
                message_complete = deserializeExtendedLength(consumed);
                break;
            case DeserializeState::MaskingKey:
                message_complete = deserializeMaskingKey(consumed);
                break;
            case DeserializeState::Payload:
                message_complete = deserializePayload(consumed);
                break;
            case DeserializeState::Complete:
                message_complete = deserializeComplete();
                break;
            default:
                throw std::runtime_error("Invalid deserialize state");
                break;
        }
    }

    data_buffer_.erase(data_buffer_.begin(), data_buffer_.begin() + consumed);
    return message_complete;
}

/**
 * @brief 解析WebSocket帧头
 * @param consumed 已消费的数据长度
 * @return bool 消息头是否解析成功
 * 
 * 帧头格式：
 * 0                   1                   2                   3
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-------+-+-------------+-------------------------------+
 * |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
 * |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
 * |N|V|V|V|       |S|             |   (if payload len==126/127)   |
 * | |1|2|3|       |K|             |                               |
 * +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
 * 
 */
bool WebSocketMessage::deserializeHeader(size_t &consumed)
{
    if (data_buffer_.size() - consumed < HEAD_PARSE_SIZE) {
        // 帧头不完整，等待更多数据
        return false;
    }

    // 解析第1个字节：FIN(1位) + RSV(3位) + 操作码(4位)
    header_.fin_opcode = static_cast<uint8_t>(data_buffer_[consumed++]);

    // 解析第2个字节：掩码(1位) + 数据长度(7位)
    uint8_t second_byte = static_cast<uint8_t>(data_buffer_[consumed++]);
    header_.masked_ = (second_byte & MASK_MASK) != 0;
    header_.payload_len = second_byte & BASIC_LENGTH_MASK;

    if (header_.payload_len < BASIC_LENGTH) {
        expected_body_length_ = header_.payload_len;
        state_ = header_.masked_ ? DeserializeState::MaskingKey : DeserializeState::Payload;
    } else {
        expected_body_length_ = 0;
        state_ = DeserializeState::ExtendedLength;
    }

    return false;
}

bool WebSocketMessage::deserializeExtendedLength(size_t &consumed)
{
    if (header_.payload_len == 126) {
        if (data_buffer_.size() - consumed < 2) {
            return false;
        }
        // 修复序列点问题：将自增操作分开
        uint8_t first_byte = static_cast<uint8_t>(data_buffer_[consumed]);
        uint8_t second_byte = static_cast<uint8_t>(data_buffer_[consumed + 1]);
        expected_body_length_ = (static_cast<uint64_t>(first_byte) << 8) | second_byte;
        consumed += 2;
        state_ = header_.masked_ ? DeserializeState::MaskingKey : DeserializeState::Payload;
    } else {
        if (data_buffer_.size() - consumed < EXTENDED_LENGTH_PARSE_SIZE) {
            return false;
        }

        expected_body_length_ = 0;
        for (int i = 0; i < EXTENDED_LENGTH_PARSE_SIZE; i++) {
            expected_body_length_ = (expected_body_length_ << 8) | 
                            static_cast<uint8_t>(data_buffer_[consumed + i]);
        }
        consumed += EXTENDED_LENGTH_PARSE_SIZE;
        state_ = header_.masked_ ? DeserializeState::MaskingKey : DeserializeState::Payload;
    }

    return false;
}

bool WebSocketMessage::deserializeMaskingKey(size_t &consumed)
{
    if (data_buffer_.size() - consumed < MASKING_KEY_PARSE_SIZE) {
        return false;
    }

    header_.masking_key.resize(MASKING_KEY_PARSE_SIZE);
    std::memcpy(header_.masking_key.data(), data_buffer_.data() + consumed, MASKING_KEY_PARSE_SIZE);
    consumed += MASKING_KEY_PARSE_SIZE;
    state_ = DeserializeState::Payload;
    return false;
}

bool WebSocketMessage::deserializePayload(size_t &consumed)
{
    size_t remaining = data_buffer_.size() - consumed;
    size_t bytes_to_read = std::min(remaining, expected_body_length_);

    body_.insert(body_.end(), data_buffer_.begin() + consumed, data_buffer_.begin() + consumed + bytes_to_read);
    consumed += bytes_to_read;
    expected_body_length_ -= bytes_to_read;

    if (expected_body_length_ == 0) {
        state_ = DeserializeState::Complete;
    }

    return false;
}

bool WebSocketMessage::deserializeComplete()
{
    bool is_final = (header_.fin_opcode & MASK_MASK) != 0;
    if (is_final) { // 最后一帧将状态重置为初始状态，下一次解析直接覆盖
        state_ = DeserializeState::Initial;
    }
    else {  // 非最后一帧，状态切换到解析帧头继续解析
        state_ = DeserializeState::Header;
    }

    return is_final;
}

} // namespace protocol