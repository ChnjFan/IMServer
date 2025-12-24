#include "HttpServer.h"
#include <iostream>
#include <regex>
#include <sstream>
#include <algorithm>

namespace network {

/**
 * @brief Construct an HTTP server bound to the given address and port.
 *
 * Creates an HttpServer that uses the provided io_context and opens a TCP acceptor
 * bound to the specified address and port. The server is created in a stopped state.
 *
 * @param io_context Reference to the Boost.Asio io_context used for asynchronous I/O.
 * @param address IP address to bind the acceptor to (e.g., "0.0.0.0" or "127.0.0.1").
 * @param port TCP port number to bind the acceptor to.
 */
HttpServer::HttpServer(boost::asio::io_context& io_context, const std::string& address, uint16_t port)
    : io_context_(io_context),
      acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address(address), port)),
      running_(false) {
}

/**
 * @brief Shuts down the server and releases network resources.
 *
 * Ensures the acceptor is closed and any active connections are terminated so
 * the server cleans up network resources before destruction.
 */
HttpServer::~HttpServer() {
    stop();
}

/**
 * @brief Marks the server as running and begins accepting incoming TCP connections.
 *
 * If the server is already running this call has no effect. On successful start the
 * listener accept loop is initiated and a startup message with the bound address and port
 * is written to standard output.
 */
void HttpServer::start() {
    if (running_.exchange(true)) {
        return; // 已经在运行
    }
    
    // 开始接受连接
    doAccept();
    std::cout << "HTTP Server started on " << acceptor_.local_endpoint().address().to_string() 
              << ":" << acceptor_.local_endpoint().port() << std::endl;
}

/**
 * @brief Idempotently stops the HTTP server and releases network resources.
 *
 * Stops the server if it is running by marking it as not running, closing the acceptor,
 * closing and removing all active connections, and emitting a shutdown message.
 */
void HttpServer::stop() {
    if (!running_.exchange(false)) {
        return; // 已经停止
    }
    
    // 关闭acceptor
    acceptor_.close();
    
    // 关闭所有连接
    std::lock_guard<std::mutex> lock(connections_mutex_);
    for (auto conn : connections_) {
        conn->close();
    }
    connections_.clear();
    
    std::cout << "HTTP Server stopped" << std::endl;
}

/**
 * @brief Indicates whether the HTTP server is currently running.
 *
 * @return `true` if the server is running, `false` otherwise.
 */
bool HttpServer::isRunning() const {
    return running_;
}

/**
 * @brief Initiates an asynchronous accept loop to accept incoming TCP connections.
 *
 * On each accepted socket this function creates and starts a Connection, installs
 * the message and close callbacks, stores the connection in the active set, and
 * continues accepting additional connections while the server is running.
 */
void HttpServer::doAccept() {
    acceptor_.async_accept([this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
        if (ec) {
            std::cerr << "Accept error: " << ec.message() << std::endl;
            return;
        }
        
        // 创建新的连接对象
        auto conn = std::make_shared<Connection>(std::move(socket));
        
        // 设置消息处理回调
        conn->setMessageHandler([this](Connection::Ptr conn, const std::vector<char>& data) {
            handleHttpRequest(conn, data);
        });
        
        // 设置关闭处理回调
        conn->setCloseHandler([this](Connection::Ptr conn) {
            removeConnection(conn);
        });
        
        // 启动连接
        conn->start();
        
        // 添加到连接集合
        std::lock_guard<std::mutex> lock(connections_mutex_);
        connections_.insert(conn);
        
        // 继续接受下一个连接
        if (running_) {
            doAccept();
        }
    });
}

/**
 * @brief Handle completion of an asynchronous accept operation and register the new connection.
 *
 * Creates a Connection from the accepted socket, configures its message and close handlers,
 * starts the connection, adds it to the server's active-connection set, and continues accepting
 * new connections when the server is running.
 *
 * @param ec Error code returned by the accept operation; when set, the accept is aborted and the error is logged.
 * @param socket The accepted TCP socket to be wrapped in a Connection.
 */
void HttpServer::handleAccept(boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
    if (ec) {
        std::cerr << "Accept error: " << ec.message() << std::endl;
        return;
    }
    
    // 创建新的连接对象
    auto conn = std::make_shared<Connection>(std::move(socket));
    
    // 设置消息处理回调
    conn->setMessageHandler([this](Connection::Ptr conn, const std::vector<char>& data) {
        handleHttpRequest(conn, data);
    });
    
    // 设置关闭处理回调
    conn->setCloseHandler([this](Connection::Ptr conn) {
        removeConnection(conn);
    });
    
    // 启动连接
    conn->start();
    
    // 添加到连接集合
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_.insert(conn);
    
    // 继续接受下一个连接
    if (running_) {
        doAccept();
    }
}

/**
 * @brief Remove a connection from the server's tracked active connections.
 *
 * Removes the given Connection shared pointer from the internal set of
 * active connections in a thread-safe manner. If the connection is not
 * tracked, the call has no effect.
 *
 * @param conn Shared pointer to the Connection to remove.
 */
void HttpServer::removeConnection(Connection::Ptr conn) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_.erase(conn);
}

/**
 * @brief Processes raw HTTP request bytes, invokes the matching route handler, sends the HTTP response, and closes the connection.
 *
 * Parses the provided request data into an HttpRequest; if a registered route handler exists it is invoked to produce the HttpResponse, otherwise a 404 Not Found response is sent. If an exception occurs while handling the request, a 500 Internal Server Error response is sent. The connection is closed after the response is transmitted.
 *
 * @param conn Shared pointer to the Connection used to send the response; must be non-null.
 * @param data Raw HTTP request bytes to be parsed and handled.
 */
void HttpServer::handleHttpRequest(Connection::Ptr conn, const std::vector<char>& data) {
    try {
        // 解析HTTP请求
        HttpRequest req = parseHttpRequest(data);
        
        // 创建HTTP响应
        HttpResponse resp;
        
        // 查找路由处理函数
        std::lock_guard<std::mutex> lock(route_mutex_);
        auto it = route_handlers_.find(req.path);
        
        if (it != route_handlers_.end()) {
            // 调用路由处理函数
            it->second(conn, req, resp);
        } else {
            // 路由未找到
            resp.status_code = 404;
            resp.status_message = "Not Found";
            resp.body = "404 Not Found";
        }
        
        // 构建HTTP响应
        std::vector<char> response_data = buildHttpResponse(resp);
        
        // 发送响应
        conn->send(response_data);
        
        // HTTP通常是短连接，发送完响应后关闭连接
        conn->close();
    } catch (const std::exception& e) {
        std::cerr << "HTTP request handling error: " << e.what() << std::endl;
        
        // 发送500错误响应
        HttpResponse resp;
        resp.status_code = 500;
        resp.status_message = "Internal Server Error";
        resp.body = "500 Internal Server Error";
        
        std::vector<char> response_data = buildHttpResponse(resp);
        conn->send(response_data);
        conn->close();
    }
}

/**
 * @brief Parses raw HTTP request bytes into an HttpRequest structure.
 *
 * Converts the provided raw request data into an HttpRequest by extracting the
 * request line (method, path, version), splitting query parameters from the
 * path into key/value pairs, parsing header lines into the headers map, and
 * collecting the remaining bytes as the body.
 *
 * @param data Raw HTTP request bytes (typically received from a socket).
 * @return HttpRequest Populated request with fields:
 *         - method: request method (e.g., "GET", "POST")
 *         - path: request path without query string
 *         - version: HTTP version token from the request line
 *         - query_params: map of query keys to values (split by '&' and '='; no URL-decoding)
 *         - headers: header name to value map (carriage returns removed; leading space after ':' trimmed)
 *         - body: remaining payload after the blank line separating headers and body
 */
HttpRequest HttpServer::parseHttpRequest(const std::vector<char>& data) {
    HttpRequest req;
    std::string request_str(data.begin(), data.end());
    std::istringstream iss(request_str);
    std::string line;
    
    // 解析请求行
    if (std::getline(iss, line)) {
        // 移除可能的换行符
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
        
        std::istringstream line_iss(line);
        line_iss >> req.method;
        
        // 解析路径和查询参数
        std::string path_with_query;
        line_iss >> path_with_query;
        
        auto query_pos = path_with_query.find('?');
        if (query_pos != std::string::npos) {
            req.path = path_with_query.substr(0, query_pos);
            
            // 解析查询参数
            std::string query = path_with_query.substr(query_pos + 1);
            std::istringstream query_iss(query);
            std::string param;
            
            while (std::getline(query_iss, param, '&')) {
                auto equals_pos = param.find('=');
                if (equals_pos != std::string::npos) {
                    std::string key = param.substr(0, equals_pos);
                    std::string value = param.substr(equals_pos + 1);
                    req.query_params[key] = value;
                }
            }
        } else {
            req.path = path_with_query;
        }
        
        line_iss >> req.version;
    }
    
    // 解析头部信息
    while (std::getline(iss, line)) {
        // 移除可能的换行符
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
        
        // 空行表示头部结束
        if (line.empty()) {
            break;
        }
        
        auto colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);
            
            // 移除值前面的空格
            value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            }));
            
            req.headers[key] = value;
        }
    }
    
    // 解析请求体
    std::ostringstream body_oss;
    body_oss << iss.rdbuf();
    req.body = body_oss.str();
    
    return req;
}

/**
 * @brief Builds a complete HTTP response message from an HttpResponse object.
 *
 * Ensures the `Content-Length` header matches the response body size, serializes
 * the status line, headers, a blank separator line, and the body into a contiguous
 * byte buffer suitable for sending over a socket.
 *
 * @param response Source HttpResponse containing `version`, `status_code`,
 *        `status_message`, `headers`, and `body`.
 * @return std::vector<char> Raw bytes of the full HTTP response (status line,
 *         headers, CRLF separator, then body).
 */
std::vector<char> HttpServer::buildHttpResponse(const HttpResponse& response) {
    std::ostringstream oss;
    
    // 构建状态行
    oss << response.version << " " << response.status_code << " " << response.status_message << "\r\n";
    
    // 构建响应头部
    HttpResponse resp_copy = response; // 创建副本以避免修改原始对象
    
    // 设置Content-Length
    resp_copy.headers["Content-Length"] = std::to_string(resp_copy.body.size());
    
    for (const auto& header : resp_copy.headers) {
        oss << header.first << ": " << header.second << "\r\n";
    }
    
    // 空行分隔头部和响应体
    oss << "\r\n";
    
    // 添加响应体
    oss << resp_copy.body;
    
    // 转换为vector<char>
    std::string response_str = oss.str();
    return std::vector<char>(response_str.begin(), response_str.end());
}

/**
 * @brief Enable HTTPS using the provided certificate and private key files.
 *
 * Attempts to enable HTTPS for the server using the given certificate and private key file paths.
 * Currently this function is a placeholder: it performs no configuration changes and emits
 * a diagnostic message to standard error indicating that HTTPS support is not implemented.
 *
 * @param cert_file Filesystem path to the PEM-encoded certificate file.
 * @param private_key_file Filesystem path to the PEM-encoded private key file.
 */
void HttpServer::enableHttps(const std::string& cert_file, const std::string& private_key_file) {
    // TODO: 实现HTTPS支持
    std::cerr << "HTTPS support not implemented yet" << std::endl;
}

} // namespace network