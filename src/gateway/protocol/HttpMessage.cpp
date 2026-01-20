#include "HttpMessage.h"

#include <cstring>
#include <algorithm>
#include <sstream>
#include <unordered_map>

namespace protocol {

// ---------------------- HttpMessage子类实现 ----------------------

HttpMessage::HttpMessage() 
        : Message(), state_(DeserializeState::Initial)
        , is_parsing_(false), expected_body_length_(0), current_chunk_size_(0) {
    reset();
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
    body_.clear();
    data_buffer_.clear();
    expected_body_length_ = 0;
    current_chunk_size_ = 0;
    header_.headers_.clear();
    header_.method_.clear();
    header_.url_.clear();
    header_.version_.clear();
    header_.status_code_ = 0;
    header_.status_message_.clear();
}

std::vector<char> HttpMessage::serialize() const {
    std::ostringstream oss;
    
    // 构建起始行
    if (!header_.method_.empty()) {
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
                throw std::runtime_error("Invalid deserialize state");
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
        return false; // 等待更多数据
    }

    // 解析请求行或响应行
    std::string start_line(data_buffer_.begin() + consumed, data_buffer_.begin() + consumed + line_end);
    if (!parseStartLine(start_line)) {
        // 解析请求行或响应行失败直接抛出异常，应该重连
        throw std::runtime_error("Invalid HTTP message starting line");
    }

    // 移动到下一个状态
    consumed += line_end + 2; // 跳过 "\r\n"
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
    std::string headers_str(data_buffer_.begin() + consumed, data_buffer_.begin() + consumed + headers_end);
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

    consumed += headers_end + 4; // 包括 "\r\n\r\n"
    return true;
}

bool HttpMessage::deserializeHeaders(size_t &consumed)
{
    if (!parseHeaderLine(consumed)) {
        return false;// 解析未完成，等待更多数据
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
        return false; // 等待更多数据
    }

    // 读取消息体
    body_.insert(body_.end(), data_buffer_.begin() + consumed, data_buffer_.begin() + consumed + bytes_to_read);
    expected_body_length_ -= bytes_to_read;
    consumed += bytes_to_read;

    if (0 == expected_body_length_) {
        state_ = DeserializeState::Complete;
    }

    return (expected_body_length_ != 0);
}

bool HttpMessage::deserializeChunkedBodyStart(size_t &consumed)
{
    // 解析chunk大小行
    size_t chunk_size_end = std::string(data_buffer_.begin() + consumed, data_buffer_.end()).find("\r\n");
    if (chunk_size_end == std::string::npos) {
        return false; // 等待更多数据
    }
    
    // 解析chunk大小
    std::string chunk_size_str(data_buffer_.begin() + consumed, data_buffer_.begin() + consumed + chunk_size_end);
    current_chunk_size_ = std::stoull(chunk_size_str, nullptr, 16);
    
    if (current_chunk_size_ == 0) {
        // 最后一个chunk，进入chunk尾处理
        consumed += chunk_size_end + 2;
        state_ = DeserializeState::ChunkedBodyEnd;
    } else {
        // 进入chunk数据处理
        consumed += chunk_size_end + 2;
        state_ = DeserializeState::ChunkedBodyData;
    }
    return false;
}

bool HttpMessage::deserializeChunkedBodyData(size_t &consumed)
{
    // 解析chunk数据
    if (data_buffer_.size() - consumed < current_chunk_size_ + 2) { // +2 用于 "\r\n"
        return false; // 等待更多数据
    }
    
    // 读取chunk数据
    body_.insert(body_.end(), data_buffer_.begin() + consumed, data_buffer_.begin() + consumed + current_chunk_size_);
    
    consumed += current_chunk_size_ + 2; // 跳过chunk数据和 "\r\n"

    state_ = DeserializeState::ChunkedBodyStart;
    return false;
}

bool HttpMessage::deserializeChunkedBodyEnd(size_t &consumed)
{
    // 解析chunk尾的可选头部
    size_t trailer_end = std::string(data_buffer_.begin() + consumed, data_buffer_.end()).find("\r\n\r\n");
    if (trailer_end == std::string::npos) {
        return false; // 等待更多数据
    }
    
    consumed += trailer_end + 4; // 跳过 "\r\n\r\n"
    state_ = DeserializeState::Complete;
    return false;
}

} // namespace protocol