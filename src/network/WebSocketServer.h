#pragma once

#include "Connection.h"
#include <boost/asio.hpp>
#include <string>
#include <unordered_set>
#include <mutex>
#include <atomic>

/**
 * Construct a WebSocketServer bound to the given address and port using the provided io_context.
 * @param io_context Reference to the Boost.Asio io_context used for asynchronous operations.
 * @param address IP address or hostname to bind the listener to.
 * @param port TCP port to listen for incoming connections on.
 */
 
/**
 * Clean up server resources and ensure the server is stopped.
 */
 
/**
 * Start accepting incoming TCP connections and begin handling WebSocket clients.
 */
 
/**
 * Stop accepting new connections and terminate or close existing connections.
 */
 
/**
 * Register a callback to handle messages received from a connection.
 * @param handler Callable invoked when a connection delivers a message.
 */
 
/**
 * Register a callback invoked when a connection is closed.
 * @param handler Callable invoked when a connection closes.
 */
 
/**
 * Check whether the server is currently running and accepting connections.
 * @returns `true` if the server is running, `false` otherwise.
 */
 
/**
 * Begin an asynchronous accept loop to accept incoming TCP connections.
 */
 
/**
 * Handle the result of an asynchronous accept operation and take ownership of the accepted socket on success.
 * @param ec Error code describing the result of the accept operation.
 * @param socket The accepted TCP socket to be associated with a new connection on success.
 */
 
/**
 * Remove a connection from the server's internal set of active connections and release associated resources.
 * @param conn Shared pointer to the connection to remove.
 */
 
/**
 * Process WebSocket handshake data received from a connection and complete the protocol upgrade if valid.
 * @param conn Connection that sent the handshake data.
 * @param data Raw bytes containing the WebSocket handshake request.
 */
 
/**
 * Process a WebSocket data frame received from a connection and dispatch its payload to the message handler.
 * @param conn Connection that sent the frame.
 * @param data Raw bytes of the WebSocket frame.
 */
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