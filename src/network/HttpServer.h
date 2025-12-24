#pragma once

#include "Connection.h"
#include <boost/asio.hpp>
#include <unordered_map>
#include <string>
#include <mutex>
#include <functional>

/**
     * @brief Represents an HTTP request.
     *
     * Contains the method, request target, protocol version, header fields,
     * optional body, and parsed query parameters.
     *
     * Member details:
     * - method: HTTP method (e.g., "GET", "POST").
     * - path: Request path (the URI path component, excluding query string).
     * - version: HTTP protocol version (e.g., "HTTP/1.1").
     * - headers: Map of header names to values.
     * - body: Request body payload.
     * - query_params: Map of query parameter names to values parsed from the request URI.
     */
    
    /**
     * @brief Represents an HTTP response.
     *
     * Holds the protocol version, status code and message, response headers,
     * and optional body. The default constructor initializes a typical
     * successful response with sensible default headers.
     *
     * Member details:
     * - version: HTTP protocol version (default "HTTP/1.1").
     * - status_code: Numeric HTTP status code (default 200).
     * - status_message: Human-readable status reason (default "OK").
     * - headers: Map of header names to values.
     * - body: Response body payload.
     */
    
    /**
     * @brief Initializes an HttpResponse with default status and headers.
     *
     * Sets version to "HTTP/1.1", status_code to 200, status_message to "OK",
     * and adds default headers:
     * - Content-Type: "text/plain; charset=utf-8"
     * - Connection: "close"
     */
    namespace network {

// HTTP请求结构
struct HttpRequest {
    std::string method;          // HTTP方法（GET、POST等）
    std::string path;            // 请求路径
    std::string version;         // HTTP版本
    std::unordered_map<std::string, std::string> headers;  // 请求头
    std::string body;            // 请求体
    std::unordered_map<std::string, std::string> query_params;  // 查询参数
};

// HTTP响应结构
struct HttpResponse {
    std::string version;         // HTTP版本
    int status_code;             // 状态码
    std::string status_message;  // 状态消息
    std::unordered_map<std::string, std::string> headers;  // 响应头
    std::string body;            // 响应体
    
    HttpResponse() : version("HTTP/1.1"), status_code(200), status_message("OK") {
        headers["Content-Type"] = "text/plain; charset=utf-8";
        headers["Connection"] = "close";
    }
};

/**
 * Construct an HttpServer that listens on the specified address and port using the provided io_context.
 * @param io_context Reference to the Boost.Asio io_context used for asynchronous operations.
 * @param address IP address or hostname to bind the server to.
 * @param port TCP port to listen on.
 */
/**
 * Destroy the HttpServer and release any associated resources.
 */
/**
 * Start accepting incoming connections and processing HTTP requests.
 */
/**
 * Stop the server, close active connections, and cease accepting new connections.
 */
/**
 * Check whether the server is currently running.
 * @return `true` if the server is running, `false` otherwise.
 */
/**
 * Register a request handler for a specific request path.
 * The handler will be invoked with a Connection pointer, the parsed HttpRequest, and a mutable HttpResponse to fill.
 * @param path Request path to register the handler for.
 * @param handler Callable that handles requests for the given path.
 */
/**
 * Enable HTTPS using the provided certificate and private key files.
 * @param cert_file Filesystem path to the TLS certificate file (PEM format).
 * @param private_key_file Filesystem path to the TLS private key file (PEM format).
 */
/**
 * Begin an asynchronous accept operation to receive new incoming TCP connections.
 */
/**
 * Handle the completion of an asynchronous accept operation.
 * @param ec Error code indicating the result of the accept operation.
 * @param socket The newly accepted TCP socket.
 */
/**
 * Remove a connection from the server's active connection set and perform any needed cleanup.
 * @param conn Connection pointer to remove.
 */
/**
 * Process raw data received from a connection as an HTTP request and dispatch to the appropriate route handler.
 * @param conn Connection that provided the data.
 * @param data Raw bytes received from the connection.
 */
/**
 * Parse raw HTTP request bytes into an HttpRequest structure.
 * @param data Raw HTTP request bytes.
 * @return Parsed HttpRequest object.
 */
/**
 * Serialize an HttpResponse into raw bytes suitable for sending over the network.
 * @param response HttpResponse to serialize.
 * @return Vector of bytes containing the serialized HTTP response.
 */
class HttpServer {
private:
    boost::asio::io_context& io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    std::atomic<bool> running_;
    std::unordered_set<Connection::Ptr> connections_;
    std::mutex connections_mutex_;
    
    // 请求处理函数类型
    using RequestHandler = std::function<void(Connection::Ptr, const HttpRequest&, HttpResponse&)>;
    std::unordered_map<std::string, RequestHandler> route_handlers_;
    std::mutex route_mutex_;

public:
    /**
     * @brief 构造函数
     * @param io_context io_context对象引用
     * @param address 监听地址
     * @param port 监听端口
     */
    HttpServer(boost::asio::io_context& io_context, const std::string& address, uint16_t port);
    
    /**
     * @brief 析构函数
     */
    ~HttpServer();
    
    /**
     * @brief 启动服务器
     */
    void start();
    
    /**
     * @brief 停止服务器
     */
    void stop();
    
    /**
     * @brief 服务器状态检查
     * @return 服务器是否正在运行
     */
    bool isRunning() const;
    
    /**
     * @brief 注册路由处理函数
     * @param path 请求路径
     * @param handler 处理函数
     */
    template<typename Handler>
    void registerRoute(const std::string& path, Handler handler) {
        std::lock_guard<std::mutex> lock(route_mutex_);
        route_handlers_[path] = handler;
    }
    
    /**
     * @brief 启用HTTPS
     * @param cert_file 证书文件路径
     * @param private_key_file 私钥文件路径
     */
    void enableHttps(const std::string& cert_file, const std::string& private_key_file);
    
private:
    /**
     * @brief 异步接受连接
     */
    void doAccept();
    
    /**
     * @brief 处理新连接
     * @param ec 错误码
     * @param socket 新连接的socket
     */
    void handleAccept(boost::system::error_code ec, boost::asio::ip::tcp::socket socket);
    
    /**
     * @brief 移除连接
     * @param conn 要移除的连接对象
     */
    void removeConnection(Connection::Ptr conn);
    
    /**
     * @brief HTTP请求处理
     * @param conn 连接对象
     * @param data 接收到的数据
     */
    void handleHttpRequest(Connection::Ptr conn, const std::vector<char>& data);
    
    /**
     * @brief HTTP请求解析
     * @param data 原始请求数据
     * @return 解析后的HttpRequest对象
     */
    HttpRequest parseHttpRequest(const std::vector<char>& data);
    
    /**
     * @brief HTTP响应构建
     * @param response HttpResponse对象
     * @return 构建后的响应数据
     */
    std::vector<char> buildHttpResponse(const HttpResponse& response);
};

} // namespace network