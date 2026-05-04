//
// Created by Fan on 2026/4/30.
//

#include <exception>
#include <iostream>
#include <sstream>

#include "HttpConnection.h"
#include "LogicSystem.h"

void HttpConnection::UrlParser::parse(const std::string& url) {
    path_.clear();
    params_.clear();

    if (size_t queryPos = url.find('?'); queryPos != std::string::npos) {
        path_ = url.substr(0, queryPos);
        std::string queryString = url.substr(queryPos + 1);
        
        std::stringstream ss(queryString);
        std::string pair;
        while (std::getline(ss, pair, '&')) {
            if (size_t eqPos = pair.find('='); eqPos != std::string::npos) {
                std::string key = urlDecode(pair.substr(0, eqPos));
                std::string value = urlDecode(pair.substr(eqPos + 1));
                params_[key] = value;
            } else if (!pair.empty()) {
                params_[urlDecode(pair)] = "";
            }
        }
    } else {
        path_ = url;
    }
}

const std::string& HttpConnection::UrlParser::getPath() const {
    return path_;
}

const HttpConnection::UrlParams& HttpConnection::UrlParser::getParams() const {
    return params_;
}

bool HttpConnection::UrlParser::hasParam(const std::string& key) const {
    return params_.find(key) != params_.end();
}

std::string HttpConnection::UrlParser::getParam(const std::string& key, const std::string& defaultValue) const {
    if (const auto it = params_.find(key); it != params_.end()) {
        return it->second;
    }
    return defaultValue;
}

std::string HttpConnection::UrlParser::urlDecode(const std::string& encoded) {
    std::string decoded;
    for (size_t i = 0; i < encoded.size(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.size()) {
            char hex1 = encoded[i + 1];
            char hex2 = encoded[i + 2];
            int value = 0;

            if (hex1 >= '0' && hex1 <= '9') value += (hex1 - '0') << 4;
            else if (hex1 >= 'A' && hex1 <= 'F') value += (hex1 - 'A' + 10) << 4;
            else if (hex1 >= 'a' && hex1 <= 'f') value += (hex1 - 'a' + 10) << 4;

            if (hex2 >= '0' && hex2 <= '9') value += hex2 - '0';
            else if (hex2 >= 'A' && hex2 <= 'F') value += hex2 - 'A' + 10;
            else if (hex2 >= 'a' && hex2 <= 'f') value += hex2 - 'a' + 10;

            decoded += static_cast<char>(value);
            i += 2;
        } else if (encoded[i] == '+') {
            decoded += ' ';
        } else {
            decoded += encoded[i];
        }
    }
    return decoded;
}

HttpConnection::HttpConnection(tcp::socket socket) : socket_(std::move(socket)) {
}

void HttpConnection::start() {
    auto self = shared_from_this();
    http::async_read(socket_, buffer_, request_,
        [self](const boost::system::error_code& ec, size_t bytes_transfer) {
            try {
                if (ec) {
                    std::cout << "http read error: " << ec.what() << std::endl;
                    return;
                }

                // http 不需要做粘包处理，不需要使用字节参数
                boost::ignore_unused(bytes_transfer);
                self->handleRequest();
                self->checkDeadline();
            } catch (std::exception& e) {
                std::cout << e.what() << std::endl;
            }
    });
}

void HttpConnection::checkDeadline() {
    auto self = shared_from_this();
    deadline_.async_wait([self](const boost::system::error_code& ec) {
       if (!ec) {
           // todo：服务端主动关闭会造成大量 TIME_WAIT 状态的连接，占用文件描述符影响新建连接
           self->socket_.close();// 定时器超时直接关闭 socket
       }
    });
}

void HttpConnection::writeResponse() {
    auto self = shared_from_this();
    response_.content_length(response_.body().size());
    http::async_write(socket_, response_,
        [self](const boost::system::error_code& ec, size_t bytes_transfer) {
            self->socket_.shutdown(tcp::socket::shutdown_send);// 关闭发送端
            self->deadline_.cancel();// 复用 socket 事件，关闭 socket 时也要取消定时器
    });
}

void HttpConnection::handleRequest() {
    response_.version(request_.version());
    response_.keep_alive(false);
    if (request_.method() == http::verb::get) {
        urlParser_.parse(request_.target());
        if (!LogicSystem::getInstance()->handleGet(urlParser_.getPath(), shared_from_this())) {
            response_.result(http::status::not_found);
            response_.set(http::field::content_type, "text/plain");
            beast::ostream(response_.body()) << "url not found\r\n";
            writeResponse();
            return;
        }

        response_.result(http::status::ok);
        response_.set(http::field::server, "GateServer");
        writeResponse();
    }
}
