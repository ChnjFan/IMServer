#include "Message.h"

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

Message::Message() {
    // 初始化消息头
    memset(&header_, 0, sizeof(header_));
    header_.version = 1; // 默认版本
    connection_id_ = 0;
    connection_type_ = network::ConnectionType::TCP;
}

Message::Message(
    uint16_t message_type,
    const std::vector<char>& body,
    network::ConnectionId connection_id,
    network::ConnectionType connection_type) {
    // 初始化消息头
    memset(&header_, 0, sizeof(header_));
    header_.message_type = message_type;
    header_.version = 1; // 默认版本
    header_.total_length = sizeof(MessageHeader) + body.size();
    
    // 初始化消息体
    body_ = body;
    
    // 初始化连接信息
    connection_id_ = connection_id;
    connection_type_ = connection_type;
}

std::vector<char> Message::serialize() const {
    std::vector<char> buffer;
    buffer.reserve(header_.total_length);
    
    // 创建临时消息头，进行字节序转换
    MessageHeader network_header;
    
    // 字节序转换：主机字节序 -> 网络字节序
    network_header.total_length = htonl(header_.total_length); // 4字节字段
    network_header.message_type = htons(header_.message_type); // 2字节字段
    network_header.version = header_.version; // 单字节，无需转换
    network_header.reserved = header_.reserved; // 单字节，无需转换
    
    // 序列化消息头
    buffer.insert(buffer.end(), reinterpret_cast<const char*>(&network_header), 
                  reinterpret_cast<const char*>(&network_header) + sizeof(MessageHeader));
    
    // 序列化消息体
    buffer.insert(buffer.end(), body_.begin(), body_.end());
    
    return buffer;
}

bool Message::deserialize(const std::vector<char>& data) {
    if (data.size() < sizeof(MessageHeader)) {
        return false;
    }
    
    // 反序列化消息头到临时变量
    MessageHeader raw_header;
    memcpy(&raw_header, data.data(), sizeof(MessageHeader));
    
    // 字节序转换：网络字节序 -> 主机字节序
    header_.total_length = ntohl(raw_header.total_length); // 4字节字段
    header_.message_type = ntohs(raw_header.message_type); // 2字节字段
    header_.version = raw_header.version; // 单字节，无需转换
    header_.reserved = raw_header.reserved; // 单字节，无需转换
    
    // 检查消息总长度
    if (data.size() < header_.total_length) {
        return false;
    }
    
    // 反序列化消息体
    size_t body_size = header_.total_length - sizeof(MessageHeader);
    body_.assign(data.begin() + sizeof(MessageHeader), 
                 data.begin() + sizeof(MessageHeader) + body_size);
    
    return true;
}

void Message::updateTotalLength() {
    header_.total_length = sizeof(MessageHeader) + body_.size();
}

// 模板方法的实现需要在头文件中，这里只实现非模板方法

// MessageSerializer 实现
std::vector<char> MessageSerializer::serialize(const Message& message) {
    return message.serialize();
}

bool MessageSerializer::deserialize(const std::vector<char>& data, Message& message) {
    return message.deserialize(data);
}

bool MessageSerializer::deserializeHeader(const std::vector<char>& data, MessageHeader& header) {
    if (data.size() < sizeof(MessageHeader)) {
        return false;
    }
    
    memcpy(&header, data.data(), sizeof(MessageHeader));
    return true;
}

} // namespace protocol
