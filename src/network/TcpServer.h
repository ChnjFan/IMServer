#pragma once

#include "Connection.h"
#include <boost/asio.hpp>
#include <string>
#include <unordered_set>
#include <mutex>
#include <atomic>

namespace network {

class TcpServer : public std::enable_shared_from_this<TcpServer> {
private:
    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::atomic<bool> running_;
    std::unordered_set<Connection::Ptr> connections_;
    std::mutex connections_mutex_;

public:
    TcpServer(boost::asio::io_context& io_context, const std::string& address, uint16_t port);
    ~TcpServer();
    
    // 启动服务器
    void start();
    
    // 停止服务器
    void stop();
    
    // 服务器状态检查
    bool isRunning() const;
    
private:
    // 异步接受连接
    void doAccept();
    
    // 处理新连接
    void handleAccept(boost::system::error_code ec, boost::asio::ip::tcp::socket socket);
    
    // 移除连接
    void removeConnection(Connection::Ptr conn);
};

} // namespace network
