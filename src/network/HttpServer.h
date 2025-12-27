#pragma once

#include "../tool/IdGenerator.h"
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
namespace http {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;

// HTTP请求结构
using HttpRequest = http::request<http::string_body>;
using HttpResponse = http::response<http::string_body>;

// HTTP处理器函数类型
using HttpRequestHandler = std::function<void(const HttpRequest&, HttpResponse&)>;

// HTTP服务器类
class HttpServer {
public:
    using address = net::ip::address;
    using tcp = net::ip::tcp;

    explicit HttpServer(net::io_context& io_context);
    ~HttpServer();

    // 启动服务器
    void start(const address& addr, uint16_t port);

    // 停止服务器
    void stop();

    // 检查是否正在运行
    bool isRunning() const;

    // 注册路由处理器
    void registerRoute(const std::string& method, const std::string& path, HttpRequestHandler handler);

    // 注册GET/POST/PUT/DELETE等特定方法的路由
    void get(const std::string& path, HttpRequestHandler handler);
    void post(const std::string& path, HttpRequestHandler handler);
    void put(const std::string& path, HttpRequestHandler handler);
    void del(const std::string& path, HttpRequestHandler handler);

    // 设置静态文件目录
    void setStaticFileDirectory(const std::string& directory);

    // 设置跨域处理
    void enableCORS(const std::string& origin = "*");

private:
    // HTTP会话类
    class HttpSession : public std::enable_shared_from_this<HttpSession> {
    public:
        HttpSession(ConnectionId id, tcp::socket socket, HttpServer* server);
        ~HttpSession();

        // 启动会话
        void start();

        // 发送响应
        void sendResponse(const HttpResponse& response);

    private:
        // 异步读取请求
        void doRead();

        // 异步写入响应
        void doWrite(const HttpResponse& response);

        // 处理读取完成的回调
        void onRead(beast::error_code ec, std::size_t bytes_transferred);

        // 处理写入完成的回调
        void onWrite(beast::error_code ec, std::size_t bytes_transferred, bool close);

        // 处理请求
        void handleRequest(const HttpRequest& request);

        // 路由匹配
        HttpRequestHandler findHandler(const std::string& method, const std::string& path);

        // 发送错误响应
        void sendError(http::status status, const std::string& message);

        // 发送文件响应
        void sendFile(const std::string& file_path);

        // 获取MIME类型
        std::string getMimeType(const std::string& file_path);

        tcp::socket socket_;
        beast::flat_buffer buffer_;
        HttpRequest request_;
        HttpResponse response_;
        HttpServer* server_;
        bool is_writing_;

        // 超时定时器
        net::steady_timer timeout_timer_;
        
        // 连接ID
        ConnectionId connection_id_;
    };

    // 接受器
    tcp::acceptor acceptor_;
    tcp::socket socket_;

    // 运行状态
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

    // 异步接受连接
    void doAccept();

    // 路由表管理辅助方法
    void addRouteToTable(const std::string& method, const std::string& path, HttpRequestHandler handler);
    HttpRequestHandler findHandlerInTable(const std::string& method, const std::string& path);
};

} // namespace http
} // namespace network