#include "Connection.h"
#include <iostream>

namespace network {

Connection::Connection(boost::asio::ip::tcp::socket socket)
    : socket_(std::move(socket)),
      read_buffer_(1024),
      closed_(false) {
}

Connection::~Connection() {
    close();
}

void Connection::start() {
    doRead();
}

void Connection::close() {
    if (!closed_) {
        closed_ = true;
        boost::system::error_code ec;
        socket_.close(ec);
        if (close_handler_) {
            close_handler_(shared_from_this());
        }
    }
}

void Connection::send(const std::vector<char>& data) {
    if (closed_) return;
    
    bool write_in_progress = false;
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        write_in_progress = !write_buffer_.empty();
        write_buffer_.insert(write_buffer_.end(), data.begin(), data.end());
    }
    
    if (!write_in_progress) {
        doWrite();
    }
}

boost::asio::ip::tcp::endpoint Connection::getRemoteEndpoint() const {
    boost::system::error_code ec;
    auto endpoint = socket_.remote_endpoint(ec);
    if (ec) {
        return boost::asio::ip::tcp::endpoint();
    }
    return endpoint;
}

void Connection::setMessageHandler(MessageHandler handler) {
    message_handler_ = handler;
}

void Connection::setCloseHandler(CloseHandler handler) {
    close_handler_ = handler;
}

bool Connection::isClosed() const {
    return closed_;
}

void Connection::doRead() {
    auto self = shared_from_this();
    socket_.async_read_some(boost::asio::buffer(read_buffer_),
        [this, self](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
                std::vector<char> data(read_buffer_.begin(), read_buffer_.begin() + length);
                if (message_handler_) {
                    message_handler_(self, data);
                }
                doRead();
            } else {
                close();
            }
        });
}

void Connection::doWrite() {
    auto self = shared_from_this();
    boost::asio::async_write(socket_, boost::asio::buffer(write_buffer_),
        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
            if (!ec) {
                std::lock_guard<std::mutex> lock(write_mutex_);
                write_buffer_.clear();
            } else {
                close();
            }
        });
}

} // namespace network
