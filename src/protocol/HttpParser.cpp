#include "HttpParser.h"

#include <string>
#include <sstream>
#include <unordered_map>
#include <algorithm>

namespace protocol {

HttpParser::HttpParser() {
    reset();
}

void HttpParser::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    buffer_.clear();
    state_ = HttpParseState::Initial;
    content_length_ = 0;
    chunked_ = false;
    current_chunk_size_ = 0;
    message_.reset(new ::protocol::HttpMessage);
}

void HttpParser::asyncParse(
    const std::vector<char>& data,
    ParseCallback callback) {
    boost::system::error_code ec;
    std::shared_ptr<HttpMessage> result_message;
    bool message_complete = false;
    
    // 将新数据添加到缓冲区
    { // 加锁保护缓冲区
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_.insert(buffer_.end(), data.begin(), data.end());
    }
    
    while (!ec && !message_complete) {
        size_t consumed = 0;
        
        { // 加锁保护解析状态
            std::lock_guard<std::mutex> lock(mutex_);
            
            switch (state_) {
                case HttpParseState::Initial:
                    // 查找请求行或响应行结束标志 "\r\n"
                    size_t line_end = std::string(buffer_.begin(), buffer_.end()).find("\r\n");
                    if (line_end == std::string::npos) {
                        return; // 等待更多数据
                    }
                    
                    // 解析请求行或响应行
                    std::string start_line(buffer_.begin(), buffer_.begin() + line_end);
                    if (!parseStartLine(start_line, *message_)) {
                        ec = boost::system::errc::make_error_code(boost::system::errc::invalid_argument);
                        break;
                    }
                    
                    // 移动到下一个状态
                    consumed = line_end + 2; // 跳过 "\r\n"
                    buffer_.erase(buffer_.begin(), buffer_.begin() + consumed);
                    state_ = HttpParseState::Headers;
                    break;
                    
                case HttpParseState::Headers:
                    // 解析HTTP头
                    if (!parseHeaders(buffer_, consumed, *message_)) {
                        return; // 等待更多数据
                    }
                    
                    buffer_.erase(buffer_.begin(), buffer_.begin() + consumed);
                    
                    // 检查是否有消息体
                    auto content_length_it = message_->getHeaders().find("content-length");
                    auto transfer_encoding_it = message_->getHeaders().find("transfer-encoding");
                    
                    if (content_length_it != message_->getHeaders().end()) {
                        content_length_ = std::stoull(content_length_it->second);
                        state_ = HttpParseState::Body;
                    } else if (transfer_encoding_it != message_->getHeaders().end() && 
                              transfer_encoding_it->second == "chunked") {
                        chunked_ = true;
                        state_ = HttpParseState::ChunkedBodyStart;
                    } else {
                        // 没有消息体，解析完成
                        state_ = HttpParseState::Complete;
                    }
                    break;
                    
                case HttpParseState::Body:
                    // 解析固定长度的消息体
                    if (buffer_.size() < content_length_) {
                        return; // 等待更多数据
                    }
                    
                    // 读取消息体
                    message_->getBody().assign(buffer_.begin(), buffer_.begin() + content_length_);
                    consumed = content_length_;
                    buffer_.erase(buffer_.begin(), buffer_.begin() + consumed);
                    
                    state_ = HttpParseState::Complete;
                    break;
                    
                case HttpParseState::ChunkedBodyStart:
                    // 解析chunk大小行
                    size_t chunk_size_end = std::string(buffer_.begin(), buffer_.end()).find("\r\n");
                    if (chunk_size_end == std::string::npos) {
                        return; // 等待更多数据
                    }
                    
                    // 解析chunk大小
                    std::string chunk_size_str(buffer_.begin(), buffer_.begin() + chunk_size_end);
                    current_chunk_size_ = std::stoull(chunk_size_str, nullptr, 16);
                    
                    if (current_chunk_size_ == 0) {
                        // 最后一个chunk，进入chunk尾处理
                        consumed = chunk_size_end + 2;
                        buffer_.erase(buffer_.begin(), buffer_.begin() + consumed);
                        state_ = HttpParseState::ChunkedBodyEnd;
                    } else {
                        // 进入chunk数据处理
                        consumed = chunk_size_end + 2;
                        buffer_.erase(buffer_.begin(), buffer_.begin() + consumed);
                        state_ = HttpParseState::ChunkedBodyData;
                    }
                    break;
                    
                case HttpParseState::ChunkedBodyData:
                    // 解析chunk数据
                    if (buffer_.size() < current_chunk_size_ + 2) { // +2 用于 "\r\n"
                        return; // 等待更多数据
                    }
                    
                    // 读取chunk数据
                    message_->getBody().insert(message_->getBody().end(), buffer_.begin(), buffer_.begin() + current_chunk_size_);
                    
                    consumed = current_chunk_size_ + 2; // 跳过chunk数据和 "\r\n"
                    buffer_.erase(buffer_.begin(), buffer_.begin() + consumed);
                    
                    state_ = HttpParseState::ChunkedBodyStart;
                    break;
                    
                case HttpParseState::ChunkedBodyEnd:
                    // 解析chunk尾的可选头部
                    size_t trailer_end = std::string(buffer_.begin(), buffer_.end()).find("\r\n\r\n");
                    if (trailer_end == std::string::npos) {
                        return; // 等待更多数据
                    }
                    
                    consumed = trailer_end + 4; // 跳过 "\r\n\r\n"
                    buffer_.erase(buffer_.begin(), buffer_.begin() + consumed);
                    
                    state_ = HttpParseState::Complete;
                    break;
                    
                case HttpParseState::Complete:
                    // 构建结果消息
                    if (message_->getMessageType() == 0x01) { // HTTP请求
                        result_message = std::make_shared<::protocol::HttpMessage>(
                            message_->getMethod(),
                            message_->getUrl(),
                            message_->getVersion(),
                            message_->getHeaders(),
                            message_->getBody(),
                            connection_id_
                        );
                    } else { // HTTP响应
                        result_message = std::make_shared<::protocol::HttpMessage>(
                            message_->getVersion(),
                            message_->getStatusCode(),
                            message_->getStatusMessage(),
                            message_->getHeaders(),
                            message_->getBody(),
                            connection_id_
                        );
                    }
                    
                    message_complete = true;
                    break;
                    
                default:
                    ec = boost::system::errc::make_error_code(boost::system::errc::invalid_argument);
                    break;
            }
        }
    }
    
    // 回调通知
    callback(ec, std::move(*result_message));
}

bool HttpParser::parseStartLine(const std::string& start_line, ::protocol::HttpMessage& message) {
    // 解析HTTP请求行或响应行
    std::istringstream iss(start_line);
    std::string first, second, third;
    
    if (!(iss >> first >> second >> third)) {
        return false;
    }
    
    // 判断是请求还是响应
    if (third == "HTTP/1.1" || third == "HTTP/1.0") {
        // 请求行：METHOD URL VERSION
        message.setMethod(first);
        message.setUrl(second);
        message.setVersion(third);
        message.setMessageType(0x01); // HTTP请求
    } else if (first == "HTTP/1.1" || first == "HTTP/1.0") {
        // 响应行：VERSION STATUS_CODE STATUS_MESSAGE
        message.setVersion(first);
        message.setStatusCode(std::stoi(second));
        message.setStatusMessage(third);
        // 读取剩余的状态消息
        std::string rest;
        if (std::getline(iss, rest)) {
            message.setStatusMessage(message.getStatusMessage() + rest);
        }
        message.setMessageType(0x02); // HTTP响应
    } else {
        return false;
    }
    
    return true;
}

bool HttpParser::parseHeaders(const std::vector<char>& buffer, size_t& consumed, ::protocol::HttpMessage& message) {
    // 查找头字段结束标志 "\r\n\r\n"
    size_t headers_end = std::string(buffer.begin(), buffer.end()).find("\r\n\r\n");
    if (headers_end == std::string::npos) {
        return false; // 等待更多数据
    }
    
    // 解析头字段
    std::string headers_str(buffer.begin(), buffer.begin() + headers_end);
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
        
        message.getHeaders()[name] = value;
    }
    
    consumed = headers_end + 4; // 包括 "\r\n\r\n"
    return true;
}

} // namespace protocol
