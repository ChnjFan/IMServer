#include "HttpParser.h"

#include <string>
#include <sstream>
#include <unordered_map>
#include <algorithm>

namespace protocol {

HttpParser::HttpParser() : is_parsing_(false), parsed_message_() {
    parsed_message_.bindConnection(connection_id_, network::ConnectionType::HTTP);
}

void HttpParser::reset() {
    is_parsing_ = false;
    parsed_message_.reset();
    parsed_message_.bindConnection(connection_id_, network::ConnectionType::HTTP);
}

void HttpParser::asyncParse(const std::vector<char>& data, ParseCallback callback) {
    boost::system::error_code ec;
    std::shared_ptr<HttpMessage> result_message;
    bool message_complete = false;
    
    // 将新数据添加到缓冲区
    { // 加锁保护缓冲区
        std::lock_guard<std::mutex> lock(mutex_);

        if (!is_parsing_) {
            reset();
            is_parsing_ = true;
        }

        try {
            is_parsing_ = !parsed_message_.deserialize(data);
        }
        catch(const std::exception& e) {
            std::cerr << e.what() << '\n';
            reset();
            ec = boost::system::errc::make_error_code(boost::system::errc::invalid_argument);
        }
    }
    
    if (!is_parsing_) { // 解析完成再调回调
        callback(ec, parsed_message_);
    }
}

} // namespace protocol
