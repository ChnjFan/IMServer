#pragma once

#include "ConnectionManager.h"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/steady_timer.hpp>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <map>

namespace network {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;

// HTTP请求响应结构
using HttpRequest = http::request<http::string_body>;
using HttpResponse = http::response<http::string_body>;

// HTTP处理器函数类型
using HttpRequestHandler = std::function<void(const HttpRequest&, HttpResponse&)>;

class HttpServer;

/**
 * @brief HTTP连接类
 * 
 * 该类封装了一个HTTP连接，包括连接ID、类型、状态、统计信息等。
 * 它是所有具体连接类型（如TCP、WebSocket、Http）的基类，提供了统一的接口。
 */
class HttpConnection : public network::Connection {
public:
    using Ptr = std::shared_ptr<HttpConnection>;

private:
    boost::asio::ip::tcp::socket socket_;
    beast::flat_buffer buffer_;
    HttpRequest request_;
    HttpResponse response_;
    HttpServer* server_;
    bool is_writing_;

    // 超时定时器
    net::steady_timer timeout_timer_;

public:
    HttpConnection(ConnectionId id, boost::asio::ip::tcp::socket socket, HttpServer* server);
    ~HttpConnection();

    void start() override;
    void close() override;
    void forceClose() override;

    /**
     * @brief 发送HTTP响应
     * 
     * 该函数用于发送HTTP响应到客户端。它会将响应转换为字符串并发送。
     * 
     * @param response 要发送的HTTP响应对象
     */
    void sendResponse(const HttpResponse& response);
    void send(const std::vector<char>& data) override;
    void send(const std::string& data) override;
    void send(std::vector<char>&& data) override;

    std::string getRemoteAddress() const override;
    uint16_t getRemotePort() const override;
    boost::asio::ip::tcp::endpoint getRemoteEndpoint() const override;

    bool isConnected() const override;

private:
    /**
     * @brief 异步读取HTTP请求
     * 
     * 该函数用于异步读取客户端发送的HTTP请求。它会将请求解析到request_成员变量中。
     */
    void doRead();
    void onRead(beast::error_code ec, std::size_t bytes_transferred);

    /**
     * @brief 异步写入HTTP响应
     * 
     * 该函数用于异步写入HTTP响应到客户端。它会将响应转换为字符串并发送。
     * 
     * @param response 要发送的HTTP响应对象
     */
    void doWrite(const HttpResponse& response);
    void onWrite(beast::error_code ec, std::size_t bytes_transferred, bool close);

    /**
     * @brief 处理HTTP请求
     * 
     * 该函数用于处理客户端发送的HTTP请求。它会根据请求方法和路径调用相应的处理器函数。
     * 
     * @param request 客户端发送的HTTP请求对象
     */
    void handleRequest(const HttpRequest& request);

    /**
     * @brief 查找HTTP请求处理器
     * 
     * 该函数用于根据请求方法和路径查找对应的处理器函数。
     * 
     * @param method HTTP请求方法（如GET、POST等）
     * @param path HTTP请求路径
     * @return HttpRequestHandler 找到的处理器函数，若未找到则返回nullptr
     */
    HttpRequestHandler findHandler(const std::string& method, const std::string& path);

    void sendError(http::status status, const std::string& message);
    void sendFile(const std::string& file_path);

    std::string getMimeType(const std::string& file_path);
};

/**
 * @brief HTTP服务器类
 * 
 * 该类实现了一个基于Boost.Asio的HTTP服务器。它可以监听指定地址和端口，接受客户端连接，
 * 解析HTTP请求，调用注册的路由处理器函数处理请求，最后发送响应给客户端。
 */
class HttpServer {
public:
    using address = net::ip::address;

private:
    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    ConnectionManager& connection_manager_;

    bool running_;

    // 优化的路由表结构
    // 第一层：按HTTP方法分组
    // 第二层：按路径存储处理器（使用unordered_map实现O(1)查找）
    using PathHandlerMap = std::unordered_map<std::string, HttpRequestHandler>;
    std::unordered_map<std::string, PathHandlerMap> route_tables_;

    // 静态文件目录
    std::string static_file_directory_;

    // CORS设置
    bool cors_enabled_;
    std::string cors_origin_;

    // 连接回调
    Connection::MessageHandler message_handler_;
    Connection::StateChangeHandler state_change_handler_;
    Connection::CloseHandler close_handler_;

public:
    explicit HttpServer(net::io_context& io_context, ConnectionManager& connection_manager, const std::string& address, uint16_t port);
    ~HttpServer();

    void start();
    void stop();

    bool isRunning() const;

    void registerRoute(const std::string& method, const std::string& path, HttpRequestHandler handler);

    /**
     * @brief 注册路由
     * 
     * 该函数用于注册一个路由，当客户端发送指定方法的请求到指定路径时，会调用该路由的处理器函数。
     * 
     * @param path HTTP请求路径
     * @param handler 处理请求的处理器函数
     */
    void get(const std::string& path, HttpRequestHandler handler);
    void post(const std::string& path, HttpRequestHandler handler);
    void put(const std::string& path, HttpRequestHandler handler);
    void del(const std::string& path, HttpRequestHandler handler);

    /**
     * @brief 设置静态文件目录
     * 
     * 该函数用于设置服务器的静态文件目录。当客户端请求静态文件时，服务器会从该目录中读取文件内容并发送给客户端。
     * 
     * @param directory 静态文件目录的路径
     */
    void setStaticFileDirectory(const std::string& directory);
    std::string &getStaticFileDirectory();

    /**
     * @brief 启用跨域资源共享（CORS）
     * 
     * 该函数用于启用服务器的跨域资源共享功能。当客户端从不同域名或端口请求资源时，
     * 服务器会根据设置的允许来源（origin）发送适当的CORS响应头。
     * 
     * @param origin 允许的来源域名，默认值为"*"，表示允许所有来源
     */
    void enableCORS(const std::string &origin = "*");
    bool isCORSEnabled() const;
    std::string getCORSOrigin() const;

    HttpRequestHandler findHandlerInTable(const std::string& method, const std::string& path);

    /**
     * @brief 设置连接回调
     * 
     * 该函数用于设置连接回调函数，当有新的连接建立或连接状态改变时，会调用该回调函数。
     * 
     * @param handler 连接回调函数
     */
    void setMessageHandler(Connection::MessageHandler handler);
    void setStateChangeHandler(Connection::StateChangeHandler handler);
    void setCloseHandler(Connection::CloseHandler handler);

private:
    void doAccept();

    void addRouteToTable(const std::string& method, const std::string& path, HttpRequestHandler handler);
};

} // namespace network