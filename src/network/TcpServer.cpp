#include "TcpServer.h"
#include <iostream>

namespace network {

TcpServer::TcpServer(boost::asio::io_context& io_context, const std::string& address, uint16_t port)
    : io_context_(io_context),
      acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address(address), port)),
      running_(false) {
}

TcpServer::~TcpServer() {
    stop();
}

void TcpServer::start() {
    if (!running_) {
        running_ = true;
        doAccept();
    }
}

void TcpServer::stop() {
    if (running_) {
        running_ = false;
        boost::system::error_code ec;
        acceptor_.close(ec);
        
        std::lock_guard<std::mutex> lock(connections_mutex_);
        for (auto& conn : connections_) {
            conn->close();
        }
        connections_.clear();
    }
}

void TcpServer::setMessageHandler(Connection::MessageHandler handler) {
    message_handler_ = handler;
}

void TcpServer::setCloseHandler(Connection::CloseHandler handler) {
    close_handler_ = handler;
}

bool TcpServer::isRunning() const {
    return running_;
}

void TcpServer::doAccept() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
            if (!ec && running_) {
                handleAccept(ec, std::move(socket));
                doAccept();
            }
        });
}

void TcpServer::handleAccept(boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
    if (!ec) {
        auto conn = std::make_shared<Connection>(std::move(socket));
        conn->setMessageHandler(message_handler_);
        
        auto self = shared_from_this();
        conn->setCloseHandler([this, self](Connection::Ptr conn) {
            removeConnection(conn);
            if (close_handler_) {
                close_handler_(conn);
            }
        });
        
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            connections_.insert(conn);
        }
        
        conn->start();
    }
}

void TcpServer::removeConnection(Connection::Ptr conn) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_.erase(conn);
}

} // namespace network
