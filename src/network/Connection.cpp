/**
 * @file Connection.cpp
 * @brief 连接管理类的实现
 */
#include "Connection.h"
#include <iostream>

namespace im {
namespace network {

Connection::Connection() {
    // 注册连接事件
    connectionListenerId_ = EventSystem::getInstance().subscribe<ConnectionEvent>(
        [this](const std::shared_ptr<ConnectionEvent>& event) {
            // 触发链接事件后开始异步读取数据
            startRead(event->getSocket());
        });
}

void Connection::startRead(boost::asio::ip::tcp::socket& socket) {
    socket.async_read_some(boost::asio::buffer(read_buffer_),
        [this, self](const boost::system::error_code& ec, size_t bytes_transferred) {
            handleRead(ec, bytes_transferred);
        });
}

void Connection::handleRead(const boost::system::error_code& ec, size_t bytes_transferred) {
    if (!ec) {
        // 读取成功，处理接收到的数据
        std::vector<char> data(read_buffer_.begin(), read_buffer_.begin() + bytes_transferred);
        message_handler_(shared_from_this(), data);
        
        // 继续读取下一批数据
        startRead();
    } else {
        // 发生错误，关闭连接
        std::cerr << "[Connection] Read error: " << ec.message() << std::endl;
        close();
    }
}

void Connection::send(const std::vector<char>& data) {
    auto self(shared_from_this());
    bool write_in_progress = false;
    
    // 向写入队列添加数据
    {
        write_queue_.push(data);
        write_in_progress = writing_;
        writing_ = true;
    }
    
    // 如果当前没有写入操作，开始写入
    if (!write_in_progress) {
        startWrite();
    }
}

void Connection::startWrite() {
    auto self(shared_from_this());
    
    // 开始异步写入队列中的第一个数据
    boost::asio::async_write(socket_, boost::asio::buffer(write_queue_.front()),
        [this, self](const boost::system::error_code& ec, size_t bytes_transferred) {
            handleWrite(ec, bytes_transferred);
        });
}

void Connection::handleWrite(const boost::system::error_code& ec, size_t bytes_transferred) {
    if (!ec) {
        // 写入成功，移除已写入的数据
        write_queue_.pop();
        
        // 如果队列中还有数据，继续写入
        if (!write_queue_.empty()) {
            startWrite();
        } else {
            writing_ = false;
        }
    } else {
        // 发生错误，关闭连接
        std::cerr << "[Connection] Write error: " << ec.message() << std::endl;
        close();
    }
}

void Connection::close() {
    // 关闭socket
    boost::system::error_code ec;
    socket_.close(ec);
    
    if (ec) {
        std::cerr << "[Connection] Close error: " << ec.message() << std::endl;
    }
    
    // 调用关闭处理程序
    if (close_handler_) {
        close_handler_(shared_from_this());
    }
}

boost::asio::ip::tcp::endpoint Connection::getRemoteEndpoint() const {
    try {
        return socket_.remote_endpoint();
    } catch (const boost::system::system_error& e) {
        std::cerr << "[Connection] Get remote endpoint error: " << e.what() << std::endl;
        return boost::asio::ip::tcp::endpoint();
    }
}

} // namespace network
} // namespace im