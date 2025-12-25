#pragma once

#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio.hpp>
#include <string>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <functional>
#include <memory>

namespace network {
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;

// WebSocket连接会话类
class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
public:
    using Ptr = std::shared_ptr<WebSocketSession>;
    using MessageHandler = std::function<void(Ptr, const std::vector<char>&)>;
    using CloseHandler = std::function<void(Ptr)>;

private:
    websocket::stream<asio::ip::tcp::socket> ws_;
    beast::flat_buffer buffer_;
    MessageHandler message_handler_;
    CloseHandler close_handler_;

public:
    WebSocketSession(asio::ip::tcp::socket socket);
    ~WebSocketSession();

    // 启动会话（执行WebSocket握手）
    void start();

    // 发送文本消息
    void sendText(const std::string& message);

    // 发送二进制消息
    void sendBinary(const std::vector<char>& data);

    // 发送消息
    void send(const std::vector<char>& data);

    // 关闭连接
    void close();

    // 设置消息处理器
    void setMessageHandler(MessageHandler handler);

    // 设置关闭处理器
    void setCloseHandler(CloseHandler handler);

private:
    // 异步读取消息
    void doRead();

    // 处理读取完成
    void onRead(beast::error_code ec, std::size_t bytes_transferred);

    // 异步写入消息
    void doWrite(const std::vector<char>& data);

    // 处理写入完成
    void onWrite(beast::error_code ec, std::size_t bytes_transferred);

    // 异步关闭连接
    void doClose();

    // 处理连接关闭
    void onClose(beast::error_code ec);
};

// WebSocket服务器类
class WebSocketServer : public std::enable_shared_from_this<WebSocketServer> {
private:
    asio::io_context& io_context_;
    asio::ip::tcp::acceptor acceptor_;
    std::atomic<bool> running_;
    std::unordered_set<WebSocketSession::Ptr> connections_;
    std::mutex connections_mutex_;

public:
    WebSocketServer(asio::io_context& io_context, const std::string& address, uint16_t port);
    ~WebSocketServer();

    // 启动服务器
    void start();

    // 停止服务器
    void stop();

    // 服务器状态检查
    bool isRunning() const;

    // 广播消息到所有连接
    void broadcastText(const std::string& message);
    void broadcastBinary(const std::vector<char>& data);

private:
    // 异步接受连接
    void doAccept();

    // 处理新连接
    void onAccept(beast::error_code ec, asio::ip::tcp::socket socket);

    // 添加连接
    void addConnection(WebSocketSession::Ptr session);

    // 移除连接
    void removeConnection(WebSocketSession::Ptr session);
};

} // namespace network