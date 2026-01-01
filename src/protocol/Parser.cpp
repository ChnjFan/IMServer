#include "Parser.h"

#include <cstring>
#include <algorithm>
#include <iostream>

// 字节序转换函数所需的头文件
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

namespace protocol {

// -------------- ParserFactory 实现 --------------

ParserFactory::ParserFactory() {
    // 注册默认解析器
    registerParser(network::ConnectionType::TCP, []() {
        return std::make_shared<TcpParser>();
    });
    
    registerParser(network::ConnectionType::WebSocket, []() {
        return std::make_shared<WebSocketParser>();
    });
    
    registerParser(network::ConnectionType::HTTP, []() {
        return std::make_shared<HttpParser>();
    });
}

ParserFactory& ParserFactory::instance() {
    static ParserFactory instance;
    return instance;
}

std::shared_ptr<Parser> ParserFactory::createParser(network::ConnectionType connection_type) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 查找解析器创建函数
    auto it = parser_creators_.find(connection_type);
    if (it == parser_creators_.end()) {
        return nullptr;
    }
    
    // 尝试从缓存获取实例
    auto cache_it = parser_instances_.find(connection_type);
    if (cache_it != parser_instances_.end()) {
        auto parser = cache_it->second.lock();
        if (parser) {
            parser->reset();
            return parser;
        }
    }
    
    // 创建新实例
    auto parser = it->second();
    parser_instances_[connection_type] = parser;
    
    return parser;
}

void ParserFactory::registerParser(
    network::ConnectionType connection_type,
    std::function<std::shared_ptr<Parser>()> creator) {
    std::lock_guard<std::mutex> lock(mutex_);
    parser_creators_[connection_type] = std::move(creator);
}

// -------------- TcpParser 实现 --------------

TcpParser::TcpParser() : state_(ParseState::Header), expected_body_length_(0) {
    reset();
}

void TcpParser::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = ParseState::Header;
    memset(&header_, 0, sizeof(header_));
    body_buffer_.clear();
    expected_body_length_ = 0;
}

void TcpParser::asyncParse(
    const std::vector<char>& data,
    ParseCallback callback) {
    std::vector<char> buffer = data;
    size_t consumed = 0;
    boost::system::error_code ec;
    Message message;
    
    { // 加锁保护解析状态
        std::lock_guard<std::mutex> lock(mutex_);
        
        while (consumed < buffer.size()) {
            size_t bytes_processed = 0;
            bool message_complete = false;
            
            switch (state_) {
                case ParseState::Header:
                    message_complete = parseHeader(buffer, consumed);
                    break;
                    
                case ParseState::Body:
                    message_complete = parseBody(buffer, consumed);
                    break;
                    
                default:
                    ec = boost::system::errc::make_error_code(boost::system::errc::invalid_argument);
                    break;
            }
            
            if (ec) {
                break;
            }
            
            if (message_complete) {
                // 构造完整消息
                message.getHeader() = header_;
                message.getBody() = std::move(body_buffer_);
                message.setConnectionId(connection_id_);
                message.setConnectionType(network::ConnectionType::TCP);
                
                // 重置状态，准备解析下一条消息
                reset();
                break;
            }
            
            consumed += bytes_processed;
        }
    }
    
    // 回调通知
    callback(ec, std::move(message));
}

bool TcpParser::parseHeader(const std::vector<char>& data, size_t& consumed) {
    size_t remaining = data.size() - consumed;
    if (remaining < sizeof(MessageHeader)) {
        return false;
    }
    
    // 解析消息头到临时缓冲区
    MessageHeader raw_header;
    memcpy(&header_, data.data() + consumed, sizeof(MessageHeader));
    consumed += sizeof(MessageHeader);
    // 字节序转换：网络字节序 -> 主机字节序
    // 注意：只有多字节字段需要转换，单字节字段（version, reserved）不需要
    header_.total_length = ntohl(header_.total_length); // 4字节字段
    header_.message_type = ntohs(header_.message_type); // 2字节字段
    
    // 计算消息体长度
    expected_body_length_ = header_.total_length - sizeof(MessageHeader);
    
    // 切换到解析消息体状态
    state_ = ParseState::Body;
    
    return false; // 消息未完成
}

bool TcpParser::parseBody(const std::vector<char>& data, size_t& consumed) {
    size_t remaining = data.size() - consumed;
    size_t bytes_needed = expected_body_length_ - body_buffer_.size();
    size_t bytes_to_read = std::min(remaining, bytes_needed);
    
    // 读取消息体数据
    body_buffer_.insert(body_buffer_.end(), 
                       data.begin() + consumed, 
                       data.begin() + consumed + bytes_to_read);
    consumed += bytes_to_read;
    
    // 检查是否完成
    if (body_buffer_.size() >= expected_body_length_) {
        return true; // 消息完成
    }
    
    return false; // 消息未完成
}

// -------------- WebSocketParser 实现 --------------

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
    payload_length_ = 0;
    masking_key_.clear();
}

void WebSocketParser::asyncParse(
    const std::vector<char>& data,
    ParseCallback callback) {
    // 简化实现，实际需要解析WebSocket帧
    boost::system::error_code ec;
    Message message;
    
    // 这里只是一个简化实现，实际需要完整的WebSocket帧解析
    message.getHeader().message_type = 0; // 未知消息类型
    message.getBody() = data;
    message.setConnectionId(connection_id_);
    message.setConnectionType(network::ConnectionType::WebSocket);
    
    callback(ec, std::move(message));
}

// -------------- HttpParser 实现 --------------

HttpParser::HttpParser() {
    reset();
}

void HttpParser::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    buffer_.clear();
}

void HttpParser::asyncParse(
    const std::vector<char>& data,
    ParseCallback callback) {
    // 简化实现，实际需要解析HTTP请求
    boost::system::error_code ec;
    Message message;
    
    // 这里只是一个简化实现，实际需要完整的HTTP请求解析
    message.getHeader().message_type = 0; // 未知消息类型
    message.getBody() = data;
    message.setConnectionId(connection_id_);
    message.setConnectionType(network::ConnectionType::HTTP);
    
    callback(ec, std::move(message));
}

} // namespace protocol
