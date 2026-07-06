//
// Created by Fan on 2026/7/2.
//

#include <exception>
#include <iostream>
#include <sstream>

#include "HttpConnection.h"
#include "../core/ResourceLogicSystem.h"


HttpConnection::HttpConnection(boost::asio::io_context &io_context)
    : socket_(io_context) {
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
                boost::ignore_unused(bytes_transfer);
                self->handleRequest();
                self->checkDeadline();
            } catch (std::exception& e) {
                std::cout << e.what() << std::endl;
            }
    });
}

const UrlParams& HttpConnection::getUrlParams() const {
    return urlParser_.getParams();
}

void HttpConnection::checkDeadline() {
    auto self = shared_from_this();
    deadline_.async_wait([self](const boost::system::error_code& ec) {
       if (!ec) {
           self->socket_.close();
       }
    });
}

void HttpConnection::writeResponse() {
    auto self = shared_from_this();
    response_.content_length(response_.body().size());
    http::async_write(socket_, response_,
        [self](const boost::system::error_code& ec, size_t bytes_transfer) {
            if (ec) {
                std::cout << "Socket is shutdown: " << ec.what() << std::endl;
            }
            else {
                try {
                    self->socket_.shutdown(tcp::socket::shutdown_send);
                } catch (std::exception &e) {
                    std::cout << "Socket shutdown error: " << e.what() << std::endl;
                }
            }
            self->deadline_.cancel();
    });
}

void HttpConnection::handleRequest() {
    response_.version(request_.version());
    response_.keep_alive(false);

    if (request_.method() == http::verb::get) {
        urlParser_.parse(request_.target());
        if (!ResourceLogicSystem::getInstance()->handleGet(urlParser_.getPath(), shared_from_this())) {
            response_.result(http::status::not_found);
            response_.set(http::field::content_type, "text/plain");
            beast::ostream(response_.body()) << "url not found\r\n";
            writeResponse();
            return;
        }

        response_.result(http::status::ok);
        response_.set(http::field::server, "ResourceServer");
        writeResponse();
    }
    else if (request_.method() == http::verb::post) {
        // For POST, route by URL path (query params ignored for routing)
        urlParser_.parse(request_.target());

        if (!ResourceLogicSystem::getInstance()->handlePost(urlParser_.getPath(), shared_from_this())) {
            response_.result(http::status::not_found);
            response_.set(http::field::content_type, "text/plain");
            beast::ostream(response_.body()) << "url not found\r\n";
            writeResponse();
            return;
        }

        response_.result(http::status::ok);
        response_.set(http::field::server, "ResourceServer");
        writeResponse();
    }
    else {
        response_.result(http::status::method_not_allowed);
        response_.set(http::field::content_type, "text/plain");
        beast::ostream(response_.body()) << "method not allowed\r\n";
        writeResponse();
    }
}
