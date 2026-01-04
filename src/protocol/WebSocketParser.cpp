#include "WebSocketParser.h"

#include <iostream>
#include <cstring>
#include <algorithm>
#include <boost/system/error_code.hpp>

namespace protocol {

WebSocketParser::WebSocketParser() : is_parsing_(false), parsed_message_() {
    parsed_message_.bindConnection(connection_id_, network::ConnectionType::WebSocket);
}

void WebSocketParser::reset() {
    is_parsing_ = false;
    parsed_message_.reset();
    parsed_message_.bindConnection(connection_id_, network::ConnectionType::WebSocket);
}

void WebSocketParser::asyncParse(const std::vector<char>& data, ParseCallback callback) {
    boost::system::error_code ec;

    {
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

    if (!is_parsing_) {
        callback(ec, parsed_message_);
    }
}

} // namespace protocol
