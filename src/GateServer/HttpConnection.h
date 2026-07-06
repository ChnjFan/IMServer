//
// Created by Fan on 2026/4/30.
//

#ifndef IMSERVER_HTTPCONNECTION_H
#define IMSERVER_HTTPCONNECTION_H

#include <memory>
#include <string>
#include <unordered_map>

#include "const.h"
#include "UrlParser.h"

class HttpConnection : public std::enable_shared_from_this<HttpConnection> {
public:
    friend class LogicSystem;

    explicit HttpConnection(boost::asio::io_context& io_context);
    void start();

    tcp::socket& getSocket() { return socket_; }

private:
    void checkDeadline();
    void writeResponse();
    void handleRequest();

    tcp::socket socket_;
    beast::flat_buffer buffer_{8192};
    http::request<http::dynamic_body> request_;
    http::response<http::dynamic_body> response_;
    net::steady_timer deadline_{socket_.get_executor(), std::chrono::seconds(60)};

    UrlParser urlParser_;
};


#endif //IMSERVER_HTTPCONNECTION_H