#include "TcpMessage.h"

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

// ---------------------- TCPMessage子类实现 ----------------------

TCPMessage::TCPMessage() : Message() {
    reset();
}

TCPMessage::TCPMessage(const std::vector<char>& body, network::ConnectionId connection_id)
     : Message(body, connection_id, network::ConnectionType::TCP) {
    reset();
}

void TCPMessage::reset()
{
    body_.clear();
    connection_id_ = 0;
    connection_type_ = network::ConnectionType::TCP;
    state_ = DeserializeState::Header;
    data_buffer_.clear();
    expected_body_length_ = 0;
}

std::vector<char> TCPMessage::serialize() const {
    std::vector<char> buffer;
    buffer.reserve(header_.total_length);
    
    // 创建临时消息头，进行字节序转换
    TcpMessageHeader network_header;
    
    // 字节序转换：主机字节序 -> 网络字节序
    network_header.total_length = htonl(header_.total_length); // 4字节字段
    network_header.message_type = htons(header_.message_type); // 2字节字段
    network_header.version = header_.version; // 单字节，无需转换
    network_header.reserved = header_.reserved; // 单字节，无需转换
    
    // 序列化消息头
    buffer.insert(buffer.end(), reinterpret_cast<const char*>(&network_header), 
                  reinterpret_cast<const char*>(&network_header) + sizeof(TcpMessageHeader));
    
    // 序列化消息体
    buffer.insert(buffer.end(), body_.begin(), body_.end());
    
    return buffer;
}

bool TCPMessage::deserialize(const std::vector<char>& data) {
    data_buffer_.insert(data_buffer_.end(), data.begin(), data.end());

    bool message_complete = false;
    size_t consumed = 0;
    while (consumed < data_buffer_.size() && !message_complete) {
        switch (state_) {
            case DeserializeState::Header:
                message_complete = deserializeHeader(consumed);
                break;
            case DeserializeState::Body:
                message_complete = deserializeBody(consumed);
                break;
            default:
                throw std::runtime_error("Invalid deserialize state");
                break;
        }
    }

    data_buffer_.erase(data_buffer_.begin(), data_buffer_.begin() + consumed);
    return message_complete;
}

bool TCPMessage::deserializeHeader(size_t &consumed)
{
    if (consumed >= data_buffer_.size()) {
        return false;
    }
    size_t remaining = data_buffer_.size() - consumed;
    if (remaining < sizeof(TcpMessageHeader)) {
        return false;
    }
    
    memcpy(&header_, data_buffer_.data() + consumed, sizeof(TcpMessageHeader));
    header_.total_length = ntohl(header_.total_length);
    header_.message_type = ntohs(header_.message_type);

    expected_body_length_ = header_.total_length - sizeof(TcpMessageHeader);

    consumed += sizeof(TcpMessageHeader);
    state_ = DeserializeState::Body;

    return false;
}

bool TCPMessage::deserializeBody(size_t &consumed)
{
    if (consumed >= data_buffer_.size()) {
        return false;
    }
    size_t remaining = data_buffer_.size() - consumed;
    size_t bytes_needed = body_.size() > expected_body_length_ ? 0 : expected_body_length_ - body_.size();
    size_t bytes_to_read = std::min(remaining, bytes_needed);

    body_.insert(body_.end(), data_buffer_.begin() + consumed, data_buffer_.begin() + consumed + bytes_to_read);
    consumed += bytes_to_read;

    return (body_.size() >= expected_body_length_);
}

} // namespace protocol