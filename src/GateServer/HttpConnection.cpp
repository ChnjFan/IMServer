//
// Created by Fan on 2026/4/30.
//

#include <exception>
#include <iostream>
#include <sstream>

#include "HttpConnection.h"
#include "LogicSystem.h"

HttpConnection::HttpConnection(boost::asio::io_context &io_context) : socket_(io_context) {
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
            if (ec) {
                std::cout << "Socket is shutdown: " << ec.what() << std::endl;
            }
            else {
                try {
                    self->socket_.shutdown(tcp::socket::shutdown_send);// 关闭发送端
                } catch (std::exception &e) {
                    std::cout << "Socket shutdown error: " << e.what() << std::endl;
                }
            }
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
    else if (request_.method() == http::verb::post) {
        if (!LogicSystem::getInstance()->handlePost(request_.target(), shared_from_this())) {
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
