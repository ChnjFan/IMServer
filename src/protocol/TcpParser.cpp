#include "TcpParser.h"

// 字节序转换函数所需的头文件
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

namespace protocol {

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
    TCPMessage message;
    
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
                message.getHeader() = header_;
                message.getBody() = std::move(body_buffer_);
                message.setConnectionId(connection_id_);

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
    
    memcpy(&header_, data.data() + consumed, sizeof(MessageHeader));
    consumed += sizeof(MessageHeader);
    // 字节序转换：网络字节序 -> 主机字节序
    header_.total_length = ntohl(header_.total_length);
    header_.message_type = ntohs(header_.message_type);

    expected_body_length_ = header_.total_length - sizeof(MessageHeader);
    state_ = ParseState::Body;

    return false;
}

bool TcpParser::parseBody(const std::vector<char>& data, size_t& consumed) {
    size_t remaining = data.size() - consumed;
    size_t bytes_needed = expected_body_length_ - body_buffer_.size();
    size_t bytes_to_read = std::min(remaining, bytes_needed);

    body_buffer_.insert(body_buffer_.end(), 
                       data.begin() + consumed, 
                       data.begin() + consumed + bytes_to_read);
    consumed += bytes_to_read;

    if (body_buffer_.size() >= expected_body_length_) {
        return true;
    }

    return false;
}

} // namespace protocol
