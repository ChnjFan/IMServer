#include "TcpParser.h"
#include <boost/system/error_code.hpp>

namespace protocol {

TcpParser::TcpParser() : is_parsing_(false), parsed_message_() {
    parsed_message_.bindConnection(connection_id_, network::ConnectionType::TCP);
}

void TcpParser::reset() {
    is_parsing_ = false;
    parsed_message_.reset();
    parsed_message_.bindConnection(connection_id_, network::ConnectionType::TCP);
}

void TcpParser::asyncParse(const std::vector<char>& data, ParseCallback callback) {
    size_t consumed = 0;
    boost::system::error_code ec;
    
    { // 加锁保护解析状态
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
