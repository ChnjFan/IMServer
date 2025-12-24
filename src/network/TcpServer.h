#pragma once

#include "Connection.h"
#include <boost/asio.hpp>
#include <string>
#include <unordered_set>
#include <mutex>
#include <atomic>

/**
 * Create a TCP server bound to the specified address and port using the provided IO context.
 * @param io_context ASIO IO context used for asynchronous operations.
 * @param address IP address or host interface to bind the server to.
 * @param port TCP port to listen on.
 */
/**
 * Clean up server resources and close any active connections.
 */

/**
 * Start accepting incoming TCP connections and begin managing active connections.
 */

/**
 * Stop accepting new connections and close all active connections.
 */

/**
 * Register a callback invoked when a connection receives a message.
 * @param handler Callback to handle incoming messages from connections.
 */

/**
 * Register a callback invoked when a connection is closed.
 * @param handler Callback to handle connection closure events.
 */

/**
 * Check whether the server is currently running.
 * @returns `true` if the server is running, `false` otherwise.
 */

/**
 * Initiate or continue asynchronous acceptance of new TCP connections.
 */

/**
 * Handle completion of an asynchronous accept operation and integrate the accepted socket.
 * @param ec Error code indicating the result of the accept operation.
 * @param socket The accepted TCP socket for the new connection.
 */

/**
 * Remove a connection from the server's active connection set.
 * @param conn Shared pointer to the connection to remove.
 */
namespace network {

class TcpServer : public std::enable_shared_from_this<TcpServer> {
private:
    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    Connection::MessageHandler message_handler_;
    Connection::CloseHandler close_handler_;
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
};

} // namespace network