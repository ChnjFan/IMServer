//
// Created by Fan on 2026/7/2.
//

#ifndef IMSERVER_HTTPCONNECTION_H
#define IMSERVER_HTTPCONNECTION_H

#include <memory>
#include <string>
#include <unordered_map>

#include "const.h"
#include "UrlParser.h"

class ResourceLogicSystem;

class HttpConnection : public std::enable_shared_from_this<HttpConnection> {
public:
    friend class ResourceLogicSystem;

    explicit HttpConnection(boost::asio::io_context& io_context);
    void start();

    tcp::socket& getSocket() { return socket_; }

    http::request<http::dynamic_body>& getRequest() { return request_; }
    http::response<http::dynamic_body>& getResponse() { return response_; }

    const UrlParams& getUrlParams() const;

private:
    void checkDeadline();
    void writeResponse();
    void handleRequest();

    tcp::socket socket_;
    beast::flat_buffer buffer_{65536};
    http::request<http::dynamic_body> request_;
    http::response<http::dynamic_body> response_;
    net::steady_timer deadline_{socket_.get_executor(), std::chrono::seconds(60)};

    UrlParser urlParser_;
};


#endif //IMSERVER_HTTPCONNECTION_H
