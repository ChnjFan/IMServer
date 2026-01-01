// 在WebSocketServer.h中添加is_open()方法到WebSocketSession类中
#pragma once

#include "ConnectionManager.h"
#include "IdGenerator.h"
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <string>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <functional>
#include <memory>
#include <queue>

namespace network {
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace http = beast::http;
namespace asio = boost::asio;

/**
 * @brief WebSocket连接会话类
 * 
 * 该类封装了WebSocket连接的会话，包括连接管理、消息处理、状态变更通知等功能。
 */
class WebSocketConnection : public Connection {
public:
    using Ptr = std::shared_ptr<WebSocketConnection>;

private:
    websocket::stream<asio::ip::tcp::socket> ws_;
    beast::flat_buffer buffer_;
    std::queue<std::vector<char>> write_queue_;
    std::mutex write_mutex_;
    std::atomic<bool> running_;

public:
    WebSocketConnection(ConnectionId id, asio::ip::tcp::socket socket);
    ~WebSocketConnection();

    void start() override;
    void close() override;
    void forceClose() override;
    void send(const std::vector<char>& data) override;
    void send(const std::string& data) override;
    void send(std::vector<char>&& data) override;

    boost::asio::ip::tcp::endpoint getRemoteEndpoint() const override;
    std::string getRemoteAddress() const override;
    uint16_t getRemotePort() const override;
    bool isConnected() const override;

    // 检查连接是否打开
    bool is_open() const {
        return ws_.next_layer().is_open();
    }

private:
    void doRead();
    void doWrite(std::vector<char>& data);
};

/**
 * @brief WebSocket服务器类
 * 
 * 该类封装了WebSocket服务器的功能，包括监听、接受连接、管理会话等。
 */
class WebSocketServer : public std::enable_shared_from_this<WebSocketServer> {
private:
    asio::io_context& io_context_;
    asio::ip::tcp::acceptor acceptor_;
    ConnectionManager& connection_manager_;
    std::atomic<bool> running_;
    std::mutex connections_mutex_;

public:
    WebSocketServer(asio::io_context& io_context, ConnectionManager& connection_manager, const std::string& address, uint16_t port);
    ~WebSocketServer();

    void start();
    void stop();
    bool isRunning() const;

private:
    void doAccept();
    void handleAccept(beast::error_code ec, asio::ip::tcp::socket socket);
};

} // namespace network