#pragma once

#include "Connection.h"
#include <boost/asio.hpp>
#include <string>
#include <unordered_set>
#include <mutex>
#include <atomic>

namespace network {

class WebSocketServer : public std::enable_shared_from_this<WebSocketServer> {
private:
    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    Connection::MessageHandler message_handler_;
    Connection::CloseHandler close_handler_;
    std::atomic<bool> running_;
    std::unordered_set<Connection::Ptr> connections_;
    std::mutex connections_mutex_;

public:
    WebSocketServer(boost::asio::io_context& io_context, const std::string& address, uint16_t port);
    ~WebSocketServer();
    
    // 启动服务器
    void start();
    
    // 停止服务器
    void stop();
    
    // 设置消息处理回调
    void setMessageHandler(Connection::MessageHandler handler);
    
    // 设置连接关闭回调
    void setCloseHandler(Connection::CloseHandler handler);
    
    // 服务器状态检查
    bool isRunning() const;
    
private:
    // 异步接受连接
    void doAccept();
    
    // 处理新连接
    void handleAccept(boost::system::error_code ec, boost::asio::ip::tcp::socket socket);
    
    // 移除连接
    void removeConnection(Connection::Ptr conn);
    
    // WebSocket握手处理
    void handleWebSocketHandshake(Connection::Ptr conn, const std::vector<char>& data);
    
    // WebSocket数据帧处理
    void handleWebSocketFrame(Connection::Ptr conn, const std::vector<char>& data);
};

} // namespace network
