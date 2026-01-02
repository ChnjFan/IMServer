#include "WebSocketParser.h"

#include <cstring>
#include <algorithm>

namespace protocol {

WebSocketParser::WebSocketParser() {
    reset();
}

void WebSocketParser::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = FrameState::Initial;
    current_frame_.clear();
    message_buffer_.clear();
    is_final_frame_ = false;
    opcode_ = 0;
    masked_ = false;
    payload_length_ = 0;
    masking_key_.clear();
}

/**
 * @brief 解析WebSocket帧头
 * @param data 待解析的数据
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
bool WebSocketParser::parseHeader(const std::vector<char> &data, size_t &consumed)
{
    if (data.size() - consumed < HEAD_PARSE_SIZE) {
        // 帧头不完整，等待更多数据
        return true;
    }

    // 解析第1个字节：FIN(1位) + RSV(3位) + 操作码(4位)
    uint8_t first_byte = static_cast<uint8_t>(data[consumed++]);
    is_final_frame_ = (first_byte & FIN_MASK) != 0;
    opcode_ = first_byte & OPCODE_MASK;

    // 解析第2个字节：掩码(1位) + 数据长度(7位)
    uint8_t second_byte = static_cast<uint8_t>(data[consumed++]);
    masked_ = (second_byte & MASK_MASK) != 0;
    uint8_t basic_length = second_byte & BASIC_LENGTH_MASK;

    if (basic_length < BASIC_LENGTH) {
        payload_length_ = basic_length;
        state_ = masked_ ? FrameState::MaskingKey : FrameState::Payload;
    } else {
        payload_length_ = 0;
        state_ = FrameState::ExtendedLength;
    }

    return false;
}

bool WebSocketParser::parseExtendedLength(const std::vector<char> &data, size_t &consumed)
{
    if (0 == payload_length_) {
        if (data.size() - consumed < HEAD_PARSE_SIZE) {
            return true;
        }
        payload_length_ = (static_cast<uint64_t>(static_cast<uint8_t>(data[consumed++])) << 8) | 
                                            static_cast<uint8_t>(data[consumed++]);
        state_ = masked_ ? FrameState::MaskingKey : FrameState::Payload;
    } else {
        if (data.size() - consumed < EXTENDED_LENGTH_PARSE_SIZE) {
            return true;
        }

        payload_length_ = 0;
        for (int i = 0; i < EXTENDED_LENGTH_PARSE_SIZE; i++) {
            payload_length_ = (payload_length_ << 8) | 
                            static_cast<uint8_t>(data[consumed + i]);
        }
        consumed += EXTENDED_LENGTH_PARSE_SIZE;
        state_ = masked_ ? FrameState::MaskingKey : FrameState::Payload;
    }

    return false;
}

bool WebSocketParser::parseMaskingKey(const std::vector<char> &data, size_t &consumed)
{
    if (data.size() - consumed < MASKING_KEY_PARSE_SIZE) {
        return true;
    }

    masking_key_.resize(MASKING_KEY_PARSE_SIZE);
    std::memcpy(masking_key_.data(), data.data() + consumed, MASKING_KEY_PARSE_SIZE);
    consumed += MASKING_KEY_PARSE_SIZE;
    state_ = FrameState::Payload;
    return false;
}

bool WebSocketParser::parsePayload(const std::vector<char> &data, size_t &consumed)
{
    size_t remaining = data.size() - consumed;
    size_t bytes_to_read = std::min(remaining, payload_length_);
    
    current_frame_.insert(current_frame_.end(), 
                        data.begin() + consumed, 
                        data.begin() + consumed + bytes_to_read);
    consumed += bytes_to_read;
    payload_length_ -= bytes_to_read;

    if (payload_length_ == 0) {
        state_ = FrameState::Complete;
    }

    return (payload_length_ != 0);
}

bool WebSocketParser::parseComplete(WebSocketMessage& message)
{
    // 解析掩码数据
    if (masking_key_.size() == MASKING_KEY_PARSE_SIZE) {
        for (size_t i = 0; i < current_frame_.size(); i++) {
            current_frame_[i] ^= masking_key_[i % MASKING_KEY_PARSE_SIZE];
        }
    }

    switch (opcode_) {
        case 0x01: // 文本帧
        case 0x02: // 二进制帧
        case 0x00: // 延续帧
            message_buffer_.insert(message_buffer_.end(), 
                                current_frame_.begin(), 
                                current_frame_.end());
            if (is_final_frame_) {
                message.setMessageType(opcode_);
                message.getBody() = std::move(message_buffer_);
                message.setConnectionId(connection_id_);
                message.setOpcode(opcode_);
                message.setIsFinal(is_final_frame_);
                reset();
                return true;
            }
            break;
        default:
            reset();
            return false;
    }

    return false;
}

void WebSocketParser::asyncParse(const std::vector<char>& data, ParseCallback callback) {
    boost::system::error_code ec;
    WebSocketMessage message;
    bool message_complete = false;
    
    size_t consumed = 0;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);

        while (consumed < data.size() && !ec && !message_complete) {
            switch (state_) {
                case FrameState::Initial:
                    reset();
                    state_ = FrameState::Header;
                    break;
                case FrameState::Header:
                    message_complete = parseHeader(data, consumed);
                    break;
                case FrameState::ExtendedLength:
                    message_complete = parseExtendedLength(data, consumed);
                    break;
                case FrameState::MaskingKey:
                    message_complete = parseMaskingKey(data, consumed);
                    break;
                case FrameState::Payload:
                    message_complete = parsePayload(data, consumed);
                    break;
                case FrameState::Complete:
                    message_complete = parseComplete(message);
                    break;
                default:
                    ec = boost::system::errc::make_error_code(boost::system::errc::invalid_argument);
                    break;
            }
        }
    }
    
    // 回调通知
    callback(ec, std::move(message));
}

} // namespace protocol
