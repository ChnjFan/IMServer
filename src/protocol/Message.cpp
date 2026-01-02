#include "Message.h"

#include <cstring>
#include <algorithm>
#include <sstream>
#include <unordered_map>

// 字节序转换函数所需的头文件
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

namespace protocol {

// ---------------------- Message基类实现 ----------------------

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

void Message::updateTotalLength() {
    header_.total_length = sizeof(MessageHeader) + body_.size();
}

// ---------------------- TCPMessage子类实现 ----------------------

TCPMessage::TCPMessage() : Message() {
    connection_type_ = network::ConnectionType::TCP;
}

TCPMessage::TCPMessage(
    uint16_t message_type,
    const std::vector<char>& body,
    network::ConnectionId connection_id) : 
    Message(message_type, body, connection_id, network::ConnectionType::TCP) {
}

std::vector<char> TCPMessage::serialize() const {
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

bool TCPMessage::deserialize(const std::vector<char>& data) {
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
    
    connection_type_ = network::ConnectionType::TCP;
    return true;
}

// ---------------------- WebSocketMessage子类实现 ----------------------

WebSocketMessage::WebSocketMessage() : Message() {
    connection_type_ = network::ConnectionType::WebSocket;
    opcode_ = 0x01; // 默认文本帧
    is_final_ = true;
}

WebSocketMessage::WebSocketMessage(
    uint16_t message_type,
    const std::vector<char>& body,
    network::ConnectionId connection_id,
    uint8_t opcode,
    bool is_final) : 
    Message(message_type, body, connection_id, network::ConnectionType::WebSocket) {
    opcode_ = opcode;
    is_final_ = is_final;
}

std::vector<char> WebSocketMessage::serialize() const {
    // WebSocket消息的序列化需要遵循WebSocket协议帧格式
    // 这里简化实现，只处理基本的文本帧
    std::vector<char> buffer;
    
    // 第1字节：FIN + RSV1 + RSV2 + RSV3 + OPCODE
    uint8_t first_byte = (is_final_ ? 0x80 : 0x00) | opcode_;
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
    // WebSocket消息的反序列化需要解析WebSocket协议帧格式
    // 这里简化实现，只处理基本的文本帧
    if (data.size() < 2) {
        return false;
    }
    
    // 解析第1字节
    uint8_t first_byte = static_cast<uint8_t>(data[0]);
    is_final_ = (first_byte & 0x80) != 0;
    opcode_ = first_byte & 0x0F;
    
    // 解析第2字节
    uint8_t second_byte = static_cast<uint8_t>(data[1]);
    bool masked = (second_byte & 0x80) != 0;
    uint8_t basic_length = second_byte & 0x7F;
    
    size_t payload_length = 0;
    size_t header_offset = 2;
    
    // 解析载荷长度
    if (basic_length <= 125) {
        payload_length = basic_length;
    } else if (basic_length == 126) {
        if (data.size() < header_offset + 2) {
            return false;
        }
        uint16_t length16 = 0;
        memcpy(&length16, data.data() + header_offset, 2);
        payload_length = ntohs(length16);
        header_offset += 2;
    } else {
        if (data.size() < header_offset + 8) {
            return false;
        }
        uint64_t length64 = 0;
        memcpy(&length64, data.data() + header_offset, 8);
        payload_length = be64toh(length64);
        header_offset += 8;
    }
    
    // 解析掩码密钥
    std::vector<char> masking_key;
    if (masked) {
        if (data.size() < header_offset + 4) {
            return false;
        }
        masking_key.resize(4);
        memcpy(masking_key.data(), data.data() + header_offset, 4);
        header_offset += 4;
    }
    
    // 解析消息体
    if (data.size() < header_offset + payload_length) {
        return false;
    }
    
    body_.assign(data.begin() + header_offset, data.begin() + header_offset + payload_length);
    
    // 应用掩码（如果有）
    if (masked) {
        for (size_t i = 0; i < body_.size(); ++i) {
            body_[i] ^= masking_key[i % 4];
        }
    }
    
    // 更新MessageHeader
    header_.message_type = opcode_;
    header_.total_length = sizeof(MessageHeader) + body_.size();
    connection_type_ = network::ConnectionType::WebSocket;
    
    return true;
}

// ---------------------- HttpMessage子类实现 ----------------------

HttpMessage::HttpMessage() : Message() {
    connection_type_ = network::ConnectionType::HTTP;
    status_code_ = 0;
}

HttpMessage::HttpMessage(
    const std::string& method,
    const std::string& url,
    const std::string& version,
    const std::unordered_map<std::string, std::string>& headers,
    const std::vector<char>& body,
    network::ConnectionId connection_id) : 
    Message(0x01, body, connection_id, network::ConnectionType::HTTP) {
    method_ = method;
    url_ = url;
    version_ = version;
    headers_ = headers;
    status_code_ = 0;
    
    // 更新消息类型为HTTP请求
    header_.message_type = 0x01;
}

HttpMessage::HttpMessage(
    const std::string& version,
    int status_code,
    const std::string& status_message,
    const std::unordered_map<std::string, std::string>& headers,
    const std::vector<char>& body,
    network::ConnectionId connection_id) : 
    Message(0x02, body, connection_id, network::ConnectionType::HTTP) {
    version_ = version;
    status_code_ = status_code;
    status_message_ = status_message;
    headers_ = headers;
    
    // 更新消息类型为HTTP响应
    header_.message_type = 0x02;
}

std::vector<char> HttpMessage::serialize() const {
    std::ostringstream oss;
    
    // 构建起始行
    if (header_.message_type == 0x01) {
        // HTTP请求：METHOD URL VERSION
        oss << method_ << " " << url_ << " " << version_ << "\r\n";
    } else {
        // HTTP响应：VERSION STATUS_CODE STATUS_MESSAGE
        oss << version_ << " " << status_code_ << " " << status_message_ << "\r\n";
    }
    
    // 构建头字段
    for (const auto& [name, value] : headers_) {
        oss << name << ": " << value << "\r\n";
    }
    
    // 添加Content-Length头
    oss << "Content-Length: " << body_.size() << "\r\n";
    
    // 空行分隔头和消息体
    oss << "\r\n";
    
    // 转换为std::vector<char>
    std::string header_str = oss.str();
    std::vector<char> buffer(header_str.begin(), header_str.end());
    
    // 添加消息体
    buffer.insert(buffer.end(), body_.begin(), body_.end());
    
    return buffer;
}

bool HttpMessage::deserialize(const std::vector<char>& data) {
    // HTTP消息的反序列化需要解析HTTP协议格式
    // 这里简化实现，只处理基本的请求和响应
    std::string data_str(data.begin(), data.end());
    std::istringstream iss(data_str);
    
    // 解析起始行
    std::string first, second, third;
    if (!(iss >> first >> second >> third)) {
        return false;
    }
    
    if (third == "HTTP/1.1" || third == "HTTP/1.0") {
        // HTTP请求
        method_ = first;
        url_ = second;
        version_ = third;
        header_.message_type = 0x01;
        status_code_ = 0;
        status_message_.clear();
    } else if (first == "HTTP/1.1" || first == "HTTP/1.0") {
        // HTTP响应
        version_ = first;
        status_code_ = std::stoi(second);
        status_message_ = third;
        method_.clear();
        url_.clear();
        header_.message_type = 0x02;
        
        // 读取剩余的状态消息
        std::string rest;
        if (std::getline(iss, rest)) {
            status_message_ += rest;
        }
    } else {
        return false;
    }
    
    // 解析头字段
    headers_.clear();
    std::string header_line;
    while (std::getline(iss, header_line) && header_line != "\r") {
        if (header_line.empty()) {
            continue;
        }
        
        size_t colon_pos = header_line.find(':');
        if (colon_pos == std::string::npos) {
            continue;
        }
        
        std::string name = header_line.substr(0, colon_pos);
        std::string value = header_line.substr(colon_pos + 1);
        
        // 去除空格
        name.erase(0, name.find_first_not_of(" \t"));
        name.erase(name.find_last_not_of(" \t\r") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t\r") + 1);
        
        headers_[name] = value;
    }
    
    // 解析消息体
    // 查找消息体的起始位置
    size_t body_start = data_str.find("\r\n\r\n");
    if (body_start == std::string::npos) {
        body_.clear();
        return true;
    }
    body_start += 4; // 跳过"\r\n\r\n"
    
    // 读取消息体
    if (body_start < data_str.size()) {
        body_.assign(data.begin() + body_start, data.end());
    } else {
        body_.clear();
    }
    
    // 更新MessageHeader
    header_.total_length = sizeof(MessageHeader) + body_.size();
    connection_type_ = network::ConnectionType::HTTP;
    
    return true;
}

// ---------------------- MessageSerializer工具类实现 ----------------------

std::vector<char> MessageSerializer::serialize(const Message& message) {
    return message.serialize();
}

std::shared_ptr<Message> MessageSerializer::deserialize(const std::vector<char>& data) {
    // 简化实现，根据连接类型创建相应的消息对象
    // 实际应用中可能需要更复杂的逻辑来判断消息类型
    
    // 先解析消息头，获取消息类型
    if (data.size() < sizeof(MessageHeader)) {
        return nullptr;
    }
    
    MessageHeader header;
    memcpy(&header, data.data(), sizeof(MessageHeader));
    
    // 字节序转换
    header.total_length = ntohl(header.total_length);
    header.message_type = ntohs(header.message_type);
    
    std::shared_ptr<Message> message;
    
    // 根据消息类型创建相应的消息对象
    switch (header.message_type) {
        case 0x01: // HTTP请求
        case 0x02: // HTTP响应
            message = std::make_shared<HttpMessage>();
            break;
        case 0x01: // WebSocket文本帧
        case 0x02: // WebSocket二进制帧
        case 0x00: // WebSocket延续帧
            message = std::make_shared<WebSocketMessage>();
            break;
        default: // TCP消息
            message = std::make_shared<TCPMessage>();
            break;
    }
    
    // 调用具体消息类型的deserialize方法
    if (message && message->deserialize(data)) {
        return message;
    }
    
    return nullptr;
}

bool MessageSerializer::deserializeHeader(const std::vector<char>& data, MessageHeader& header) {
    if (data.size() < sizeof(MessageHeader)) {
        return false;
    }
    
    memcpy(&header, data.data(), sizeof(MessageHeader));
    return true;
}

} // namespace protocol
