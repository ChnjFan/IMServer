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
    connection_id_ = 0;
    connection_type_ = network::ConnectionType::TCP;
}

Message::Message(const std::vector<char>& body,
                network::ConnectionId connection_id,
                network::ConnectionType connection_type) {
    body_ = body;
    connection_id_ = connection_id;
    connection_type_ = connection_type;
}

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
                std::throw(std::runtime_error("Invalid deserialize state"));
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

// ---------------------- WebSocketMessage子类实现 ----------------------

WebSocketMessage::WebSocketMessage() : Message(), state_(DeserializeState::Initial), payload_length_(0) {
    connection_type_ = network::ConnectionType::WebSocket;
    memset(&header_, 0, sizeof(WebSocketMessageHeader));
}

WebSocketMessage::WebSocketMessage(const std::vector<char>& body, network::ConnectionId connection_id) : 
    Message(body, connection_id, network::ConnectionType::WebSocket), payload_length_(0) {
    memset(&header_, 0, sizeof(WebSocketMessageHeader));
}

void WebSocketMessage::reset()
{
    body_.clear();
    connection_id_ = 0;
    connection_type_ = network::ConnectionType::WebSocket;
    state_ = DeserializeState::Initial;
    memset(&header_, 0, sizeof(WebSocketMessageHeader));
    data_buffer_.clear();
    expected_body_length_ = 0;
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
    bool message_complete = false;
    size_t consumed = 0;

    data_buffer_.insert(data_buffer_.end(), data.begin(), data.end());

    while (consumed < data_buffer_.size() && !message_complete) {
        switch (state_) {
            case DeserializeState::Initial:
                memset(&header_, 0, sizeof(TcpMessageHeader));
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
                message_complete = deserializeComplete(consumed);
                break;
            default:
                std::throw(std::runtime_error("Invalid deserialize state"));
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
        return true;
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
    if (0 == expected_body_length_) {
        if (data_buffer_.size() - consumed < HEAD_PARSE_SIZE) {
            return true;
        }
        expected_body_length_ = (static_cast<uint64_t>(static_cast<uint8_t>(data_buffer_[consumed++])) << 8) | 
                                            static_cast<uint8_t>(data_buffer_[consumed++]);
        state_ = header_.masked_ ? DeserializeState::MaskingKey : DeserializeState::Payload;
    } else {
        if (data_buffer_.size() - consumed < EXTENDED_LENGTH_PARSE_SIZE) {
            return true;
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
        return true;
    }

    masking_key_.resize(MASKING_KEY_PARSE_SIZE);
    std::memcpy(masking_key_.data(), data_buffer_.data() + consumed, MASKING_KEY_PARSE_SIZE);
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

    return (expected_body_length_ != 0);
}

bool WebSocketMessage::deserializeComplete(size_t &consumed)
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

// ---------------------- HttpMessage子类实现 ----------------------

HttpMessage::HttpMessage() 
        : Message(), state_(DeserializeState::Initial)
        , is_parsing_(false), expected_body_length_(0), current_chunk_size_(0) {
    connection_type_ = network::ConnectionType::HTTP;
    memset(&header_, 0, sizeof(header_));
}

HttpMessage::HttpMessage(const std::string& method, const std::string& url, const std::string& version,
                        const std::unordered_map<std::string, std::string>& headers,
                        const std::vector<char>& body, network::ConnectionId connection_id)
                        : Message(body, connection_id, network::ConnectionType::HTTP)
                        , state_(DeserializeState::Initial)
                        , is_parsing_(false)
                        , expected_body_length_(0), current_chunk_size_(0) {
    header_.method_ = method;
    header_.url_ = url;
    header_.version_ = version;
    header_.headers_ = headers;
}

HttpMessage::HttpMessage(const std::string& version, int status_code, const std::string& status_message,
                        const std::unordered_map<std::string, std::string>& headers,
                        const std::vector<char>& body,
                        network::ConnectionId connection_id)
                        : Message(body, connection_id, network::ConnectionType::HTTP)
                        , state_(DeserializeState::Initial)
                        , is_parsing_(false)
                        , expected_body_length_(0), current_chunk_size_(0) {
    header_.version_ = version;
    header_.status_code_ = status_code;
    header_.status_message_ = status_message;
    header_.headers_ = headers;
}

void HttpMessage::reset() {
    state_ = DeserializeState::Initial;
    is_parsing_ = false;
    memset(&header_, 0, sizeof(header_));
    body_.clear();
    data_buffer_.clear();
    expected_body_length_ = 0;
    current_chunk_size_ = 0;
}

std::vector<char> HttpMessage::serialize() const {
    std::ostringstream oss;
    
    // 构建起始行
    if (header_.message_type == 0x01) {
        // HTTP请求：METHOD URL VERSION
        oss << header_.method_ << " " << header_.url_ << " " << header_.version_ << "\r\n";
    } else {
        // HTTP响应：VERSION STATUS_CODE STATUS_MESSAGE
        oss << header_.version_ << " " << header_.status_code_ << " " << header_.status_message_ << "\r\n";
    }
    
    // 构建头字段
    for (const auto& [name, value] : header_.headers_) {
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
    data_buffer_.insert(data_buffer_.end(), data.begin(), data.end());

    bool message_complete = false;
    size_t consumed = 0;

    while (consumed < data_buffer_.size() && !message_complete) {
        switch (state_) {
            case DeserializeState::Initial:
                reset();
                message_complete = deserializeStartingLine(consumed);
                break;
            case DeserializeState::Headers:
                message_complete = deserializeHeaders(consumed);
                break;
            case DeserializeState::Body:
                message_complete = deserializeBody(consumed);
                break;
            case DeserializeState::ChunkedBodyStart:
                message_complete = deserializeChunkedBodyStart(consumed);
                break;
            case DeserializeState::ChunkedBodyData:
                message_complete = deserializeChunkedBodyData(consumed);
                break;
            case DeserializeState::ChunkedBodyEnd:
                message_complete = deserializeChunkedBodyEnd(consumed);
                break;
            case DeserializeState::Complete:
                message_complete = true;
                state_ = DeserializeState::Initial;
                break;
            default:
                std::throw(std::runtime_error("Invalid deserialize state"));
                break;
        }
    }

    data_buffer_.erase(data_buffer_.begin(), data_buffer_.begin() + consumed);
    return message_complete;
}

/**
 * @brief 解析HTTP消息起始行
 * @param line 起始行字符串
 * @return bool 是否成功解析
 * 
 * HTTP 请求行格式：METHOD URL VERSION
 * HTTP 响应行格式：VERSION STATUS_CODE STATUS_MESSAGE
 */
bool HttpMessage::parseStartLine(const std::string &line)
{
    // 解析HTTP请求行或响应行
    std::istringstream iss(line);
    std::string first, second, third;
    
    if (!(iss >> first >> second >> third)) {
        return false;
    }

    if (third == "HTTP/1.1" || third == "HTTP/1.0") {
        // 请求行：METHOD URL VERSION
        header_.method_ = first;
        header_.url_ = second;
        header_.version_ = third;
    } else if (first == "HTTP/1.1" || first == "HTTP/1.0") {
        // 响应行：VERSION STATUS_CODE STATUS_MESSAGE
        header_.version_ = first;
        header_.status_code_ = std::stoi(second);
        header_.status_message_ = third;
        // 读取剩余的状态消息
        std::string rest;
        if (std::getline(iss, rest)) {
            header_.status_message_ += rest;
        }
    } else {
        return false;
    }
    
    return true;
}

bool HttpMessage::deserializeStartingLine(size_t &consumed)
{
    // 查找请求行或响应行结束标志 "\r\n"
    size_t line_end = std::string(data_buffer_.begin() + consumed, data_buffer_.end()).find("\r\n");
    if (line_end == std::string::npos) {
        return true; // 等待更多数据
    }

    // 解析请求行或响应行
    std::string start_line(data_buffer_.begin() + consumed, data_buffer_.begin() + line_end);
    if (!parseStartLine(start_line)) {
        // 解析请求行或响应行失败直接抛出异常，应该重连
        throw std::runtime_error("Invalid HTTP message starting line");
    }

    // 移动到下一个状态
    consumed = line_end + 2; // 跳过 "\r\n"
    state_ = DeserializeState::Headers;
    return false;
}

bool HttpMessage::parseHeaderLine(size_t &consumed)
{
    // 查找头字段结束标志 "\r\n\r\n"
    size_t headers_end = std::string(data_buffer_.begin() + consumed, data_buffer_.end()).find("\r\n\r\n");
    if (headers_end == std::string::npos) {
        return false; // 等待更多数据
    }
    
    // 解析头字段
    std::string headers_str(data_buffer_.begin() + consumed, data_buffer_.begin() + headers_end);
    std::istringstream headers_iss(headers_str);
    std::string header_line;
    
    while (std::getline(headers_iss, header_line)) {
        // 跳过空行
        if (header_line.empty() || header_line == "\r") {
            continue;
        }

        // 解析头字段：Name: Value
        size_t colon_pos = header_line.find(':');
        if (colon_pos == std::string::npos) {
            continue;
        }
        
        std::string name = header_line.substr(0, colon_pos);
        // 去除名称前后空格，并转换为小写
        name.erase(0, name.find_first_not_of(" \t"));
        name.erase(name.find_last_not_of(" \t") + 1);
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);

        std::string value = header_line.substr(colon_pos + 1);
        // 去除值前后空格
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t\r") + 1);

        header_.headers_[name] = value;
    }

    consumed = headers_end + sizeof("\r\n\r\n") - 1; // 包括 "\r\n\r\n"
    return true;
}

bool HttpMessage::deserializeHeaders(size_t &consumed)
{
    if (!parseHeaderLine(consumed)) {
        return true;// 解析完成，等待更多数据
    }

    // 检查是否有消息体
    auto content_length_it = header_.headers_.find("content-length");
    auto transfer_encoding_it = header_.headers_.find("transfer-encoding");
    
    if (content_length_it != header_.headers_.end()) {
        expected_body_length_ = std::stoull(content_length_it->second);
        state_ = DeserializeState::Body;
    } else if (transfer_encoding_it != header_.headers_.end()
                 && transfer_encoding_it->second == "chunked") {
        state_ = DeserializeState::ChunkedBodyStart;
    } else {
        // 没有消息体，解析完成
        state_ = DeserializeState::Complete;
    }

    return false;
}

bool HttpMessage::deserializeBody(size_t &consumed)
{
    uint64_t remaining = data_buffer_.size() - consumed;
    uint64_t bytes_to_read = std::min(remaining, expected_body_length_);
    if (bytes_to_read == 0) {
        return true; // 等待更多数据
    }

    // 读取消息体
    body_.insert(body_.end(), data_buffer_.begin() + consumed, data_buffer_.begin() + consumed + bytes_to_read);
    expected_body_length_ -= bytes_to_read;
    consumed += bytes_to_read;

    if (0 == expected_body_length_) {
        state_ = DeserializeState::Complete;
    }

    return (0 != expected_body_length_);
}

bool HttpMessage::deserializeChunkedBodyStart(size_t &consumed)
{
    // 解析chunk大小行
    size_t chunk_size_end = std::string(data_buffer_.begin() + consumed, data_buffer_.end()).find("\r\n");
    if (chunk_size_end == std::string::npos) {
        return true; // 等待更多数据
    }
    
    // 解析chunk大小
    std::string chunk_size_str(data_buffer_.begin() + consumed, data_buffer_.begin() + chunk_size_end);
    current_chunk_size_ = std::stoull(chunk_size_str, nullptr, 16);
    
    if (current_chunk_size_ == 0) {
        // 最后一个chunk，进入chunk尾处理
        consumed = chunk_size_end + 2;
        state_ = DeserializeState::ChunkedBodyEnd;
    } else {
        // 进入chunk数据处理
        consumed = chunk_size_end + 2;
        state_ = DeserializeState::ChunkedBodyData;
    }
    return false;
}

bool HttpMessage::deserializeChunkedBodyData(size_t &consumed)
{
    // 解析chunk数据
    if (data_buffer_.size() - consumed < current_chunk_size_ + 2) { // +2 用于 "\r\n"
        return true; // 等待更多数据
    }
    
    // 读取chunk数据
    body_.insert(body_.end(), data_buffer_.begin() + consumed, data_buffer_.begin() + consumed + current_chunk_size_);
    
    consumed = current_chunk_size_ + 2; // 跳过chunk数据和 "\r\n"

    state_ = DeserializeState::ChunkedBodyStart;
    return false;
}

bool HttpMessage::deserializeChunkedBodyEnd(size_t &consumed)
{
    // 解析chunk尾的可选头部
    size_t trailer_end = std::string(buffer_.begin(), buffer_.end()).find("\r\n\r\n");
    if (trailer_end == std::string::npos) {
        return true; // 等待更多数据
    }
    
    consumed = trailer_end + 4; // 跳过 "\r\n\r\n"
    state_ = DeserializeState::Complete;
    return false;
}

// ---------------------- MessageSerializer工具类实现 ----------------------

std::vector<char> MessageSerializer::serialize(const Message& message) {
    return message.serialize();
}

std::shared_ptr<Message> MessageSerializer::deserialize(network::ConnectionType type, const std::vector<char> &data)
{
    std::shared_ptr<Message> message;

    switch (type) {
        case network::ConnectionType::HTTP:
            message = std::make_shared<HttpMessage>();
            break;
        case network::ConnectionType::TCP:
            message = std::make_shared<TCPMessage>();
            break;
        case network::ConnectionType::WebSocket:
            message = std::make_shared<WebSocketMessage>();
            break;
        default:
            return nullptr;
    }
    
    if (message && message->deserialize(data)) {
        return message;
    }
    
    return nullptr;
}

} // namespace protocol
