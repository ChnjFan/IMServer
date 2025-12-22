/**
 * @file TcpServer.cpp
 * @brief TCP服务器类的实现
 */

#include "TcpServer.h"
#include <iostream>

namespace im {
namespace network {

TcpServer::TcpServer(EventSystem& event_system, const std::string& host, uint16_t port)
    : event_system_(event_system), 
      acceptor_(event_system_.getIoContext(), 
                boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)) {}

void TcpServer::start() {
    // 开始异步接受连接
    startAccept();
    std::cout << "[TcpServer] Started listening on port " << acceptor_.local_endpoint().port() << std::endl;
}

void TcpServer::stop() {
    // 关闭acceptor
    boost::system::error_code ec;
    acceptor_.close(ec);
    
    if (ec) {
        std::cerr << "[TcpServer] Stop error: " << ec.message() << std::endl;
    } else {
        std::cout << "[TcpServer] Stopped listening" << std::endl;
    }
}

void TcpServer::startAccept() {
    // 异步接受新连接
    acceptor_.async_accept(
        [this](const boost::system::error_code& ec, boost::asio::ip::tcp::socket socket) {
            handleAccept(std::move(socket), ec);
        });
}

void TcpServer::handleAccept(boost::asio::ip::tcp::socket socket, const boost::system::error_code& ec) {
    if (!ec) {
        // 接受连接成功
        auto remote_endpoint = socket.remote_endpoint();
        std::cout << "[TcpServer] New connection from " << remote_endpoint.address().to_string() 
                  << ":" << remote_endpoint.port() << std::endl;
        
        // 创建Connection对象并设置回调函数
        auto connection = std::make_shared<Connection>(
            std::move(socket),
            message_handler_,
            close_handler_);
        
        // 启动连接
        connection->start();
    } else {
        // 接受连接失败
        std::cerr << "[TcpServer] Accept error: " << ec.message() << std::endl;
    }
    
    // 继续接受下一个连接
    startAccept();
}

void TcpServer::setMessageHandler(Connection::MessageHandler handler) {
    message_handler_ = std::move(handler);
}

void TcpServer::setCloseHandler(Connection::CloseHandler handler) {
    close_handler_ = std::move(handler);
}

} // namespace network
} // namespace im