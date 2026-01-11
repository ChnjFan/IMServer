#include "HttpServer.h"
#include "IdGenerator.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <thread>

namespace network {
    
HttpConnection::HttpConnection(ConnectionId id, boost::asio::ip::tcp::socket socket, HttpServer* server)
    : network::Connection(id, ConnectionType::HTTP)
    , socket_(std::move(socket))
    , server_(server)
    , is_writing_(false)
    , timeout_timer_(socket_.get_executor()) {
}

HttpConnection::~HttpConnection() {
    // 取消定时器
    timeout_timer_.cancel();
    
    // 关闭连接
    close();
}

void HttpConnection::start() {
    setState(ConnectionState::Connected);
    
    // 设置超时定时器（30秒）
    timeout_timer_.expires_after(std::chrono::seconds(30));
    timeout_timer_.async_wait([this](boost::system::error_code ec) {
        if (!ec && socket_.is_open()) {
            std::cerr << "HTTP session timeout" << std::endl;
            socket_.close();
        }
        setState(ConnectionState::Disconnected);
    });

    doRead();
}

void HttpConnection::close() {
    if (!isConnected()) {
        return;
    }
    
    boost::system::error_code ec;
    socket_.close(ec);
    
    if (ec && ec != boost::system::errc::operation_canceled) {
        std::cerr << "HTTP session close error: " << ec.message() << std::endl;
    }
}

void HttpConnection::forceClose() {
    close();
}

void HttpConnection::sendResponse(const HttpResponse& response) {
    doWrite(response);
}

void HttpConnection::send(const std::vector<char>& data) {
    // HTTP会话不直接支持原始数据发送，通过HTTP响应机制
    HttpResponse response;
    response.result(http::status::ok);
    response.set(http::field::content_type, "application/octet-stream");
    response.body() = std::string(data.begin(), data.end());
    sendResponse(response);
}

void HttpConnection::send(const std::string& data) {
    HttpResponse response;
    response.result(http::status::ok);
    response.set(http::field::content_type, "text/plain");
    response.body() = data;
    sendResponse(response);
}

void HttpConnection::send(std::vector<char>&& data) {
    HttpResponse response;
    response.result(http::status::ok);
    response.set(http::field::content_type, "application/octet-stream");
    response.body() = std::string(data.begin(), data.end());
    sendResponse(response);
}

std::string HttpConnection::getRemoteAddress() const {
    boost::system::error_code ec;
    auto endpoint = socket_.remote_endpoint(ec);
    if (ec) {
        return "";
    }
    return endpoint.address().to_string();
}

uint16_t HttpConnection::getRemotePort() const {
    boost::system::error_code ec;
    auto endpoint = socket_.remote_endpoint(ec);
    if (ec) {
        return 0;
    }
    return endpoint.port();
}

boost::asio::ip::tcp::endpoint HttpConnection::getRemoteEndpoint() const {
    boost::system::error_code ec;
    auto endpoint = socket_.remote_endpoint(ec);
    if (ec) {
        return boost::asio::ip::tcp::endpoint();
    }
    return endpoint;
}

bool HttpConnection::isConnected() const {
    return socket_.is_open();
}

void HttpConnection::doRead() {
    // 解析HTTP请求
    http::async_read(socket_, buffer_, request_,
        [this](boost::system::error_code ec, std::size_t bytes_transferred) {
            onRead(ec, bytes_transferred);
        });
}

void HttpConnection::onRead(boost::system::error_code ec, std::size_t bytes_transferred) {
    timeout_timer_.expires_after(std::chrono::seconds(30));

    if (ec) {
        if (ec != http::error::end_of_stream) {
            std::cerr << "HTTP read error: " << ec.message() << std::endl;
        }
        setState(ConnectionState::Disconnected);
        return;
    }

    // 更新统计信息
    updateBytesReceived(bytes_transferred);
    incrementMessagesReceived();

    // 处理请求
    handleRequest(request_);
}

void HttpConnection::doWrite(const HttpResponse& response) {
    is_writing_ = true;

    // 检查是否需要关闭连接
    bool close = response.need_eof();

    // 发送响应
    http::async_write(socket_, response,
        [this, close](boost::system::error_code ec, std::size_t bytes_transferred) {
            onWrite(ec, bytes_transferred, close);
        });
}

void HttpConnection::onWrite(boost::system::error_code ec, std::size_t bytes_transferred, bool close) {
    is_writing_ = false;

    if (ec) {
        std::cerr << "HTTP write error: " << ec.message() << std::endl;
        setState(ConnectionState::Disconnected);
        return;
    }

    // 更新统计信息
    updateBytesSent(bytes_transferred);
    incrementMessagesSent();

    // 如果需要关闭连接或者客户端请求关闭
    if (close || request_.keep_alive() == false) {
        setState(ConnectionState::Disconnected);
        socket_.close();
        return;
    }

    // 继续读取下一个请求
    doRead();
}

void HttpConnection::handleRequest(const HttpRequest& request) {
    try {
        // 检查是否是静态文件请求
        if (!server_->getStaticFileDirectory().empty() && request.method() == http::verb::get) {
            auto file_path = server_->getStaticFileDirectory() + std::string(request.target());
            
            // 防止目录遍历攻击
            if (file_path.find("..") != std::string::npos) {
                sendError(http::status::forbidden, "Access denied");
                return;
            }

            // 如果是目录请求，尝试查找index.html
            if (std::filesystem::is_directory(file_path)) {
                file_path += "/index.html";
            }

            // 检查文件是否存在
            if (std::filesystem::exists(file_path)) {
                sendFile(file_path);
                return;
            }
        }

        // 使用高效路由查找机制
        auto handler = findHandler(std::string(request.method_string()), std::string(request.target()));
        
        if (handler) {
            // 创建响应对象
            HttpResponse response;
            response.version(request.version());
            response.keep_alive(request.keep_alive());

            // 设置CORS头
            if (server_->isCORSEnabled()) {
                response.set(http::field::access_control_allow_origin, server_->getCORSOrigin());
                response.set(http::field::access_control_allow_methods, "GET, POST, PUT, DELETE, OPTIONS");
                response.set(http::field::access_control_allow_headers, "Content-Type, Authorization");
            }

            // 调用路由处理器
            handler(request, response);

            // 发送响应
            sendResponse(response);
        } else {
            // 路由未找到
            sendError(http::status::not_found, "404 Not Found");
        }
    } catch (const std::exception& e) {
        std::cerr << "HTTP request handling error: " << e.what() << std::endl;
        sendError(http::status::internal_server_error, "500 Internal Server Error");
    }
}

HttpRequestHandler HttpConnection::findHandler(const std::string& method, const std::string& path) {
    // 使用服务器端的高效路由查找机制
    return server_->findHandlerInTable(method, path);
}

void HttpConnection::sendError(http::status status, const std::string& message) {
    HttpResponse response;
    response.result(status);
    response.set(http::field::content_type, "text/plain");
    response.body() = message;

    if (server_->isCORSEnabled()) {
        response.set(http::field::access_control_allow_origin, server_->getCORSOrigin());
    }

    response.keep_alive(request_.keep_alive());
    sendResponse(response);
}

void HttpConnection::sendFile(const std::string& file_path) {
    try {
        std::ifstream file(file_path, std::ios::binary);
        if (!file) {
            sendError(http::status::not_found, "File not found");
            return;
        }

        // 读取文件内容
        std::ostringstream ss;
        ss << file.rdbuf();
        
        HttpResponse response;
        response.result(http::status::ok);
        response.set(http::field::content_type, getMimeType(file_path));
        response.body() = ss.str();

        if (server_->isCORSEnabled()) {
            response.set(http::field::access_control_allow_origin, server_->getCORSOrigin());
        }

        response.keep_alive(request_.keep_alive());
        sendResponse(response);
    } catch (const std::exception& e) {
        std::cerr << "Error sending file: " << e.what() << std::endl;
        sendError(http::status::internal_server_error, "Error reading file");
    }
}

std::string HttpConnection::getMimeType(const std::string& file_path) {
    static const std::unordered_map<std::string, std::string> mime_types = {
        {".html", "text/html"},
        {".htm", "text/html"},
        {".css", "text/css"},
        {".js", "application/javascript"},
        {".json", "application/json"},
        {".xml", "application/xml"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".svg", "image/svg+xml"},
        {".ico", "image/x-icon"},
        {".txt", "text/plain"},
        {".pdf", "application/pdf"},
        {".zip", "application/zip"}
    };

    // 获取文件扩展名
    auto ext_pos = file_path.find_last_of('.');
    if (ext_pos != std::string::npos) {
        std::string ext = file_path.substr(ext_pos);
        auto it = mime_types.find(ext);
        if (it != mime_types.end()) {
            return it->second;
        }
    }

    return "application/octet-stream";
}

// HttpServer实现
HttpServer::HttpServer(net::io_context& io_context, ConnectionManager& connection_manager, const std::string& address, uint16_t port)
    : io_context_(io_context),
      acceptor_(io_context, ip::tcp::endpoint(ip::address::from_string(address), port)),
      connection_manager_(connection_manager),
      running_(false),
      static_file_directory_(""),
      cors_enabled_(false),
      cors_origin_("*") {
}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::start() {
    if (running_) {
        return;
    }

    try {
        // 打开acceptor
        boost::system::error_code ec;
        acceptor_.open(boost::asio::ip::tcp::v4(), ec);
        if (ec) {
            throw std::runtime_error("Failed to open acceptor: " + ec.message());
        }

        // 设置地址重用选项
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if (ec) {
            throw std::runtime_error("Failed to set reuse_address option: " + ec.message());
        }

        // 开始监听
        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec) {
            throw std::runtime_error("Failed to listen on port: " + ec.message());
        }

        running_ = true;
        std::cout << "HTTP Server started on " << addr.to_string() << ":" << port << std::endl;

        // 开始接受连接
        doAccept();
    } catch (const std::exception& e) {
        std::cerr << "Failed to start HTTP server: " << e.what() << std::endl;
        throw;
    }
}

void HttpServer::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    // 关闭acceptor
    boost::system::error_code ec;
    acceptor_.close(ec);

    std::cout << "HTTP Server stopped" << std::endl;
}

bool HttpServer::isRunning() const {
    return running_;
}

void HttpServer::registerRoute(const std::string& method, const std::string& path, HttpRequestHandler handler) {
    addRouteToTable(method, path, std::move(handler));
}

void HttpServer::get(const std::string& path, HttpRequestHandler handler) {
    registerRoute("GET", path, std::move(handler));
}

void HttpServer::post(const std::string& path, HttpRequestHandler handler) {
    registerRoute("POST", path, std::move(handler));
}

void HttpServer::put(const std::string& path, HttpRequestHandler handler) {
    registerRoute("PUT", path, std::move(handler));
}

void HttpServer::del(const std::string& path, HttpRequestHandler handler) {
    registerRoute("DELETE", path, std::move(handler));
}

void HttpServer::setStaticFileDirectory(const std::string& directory) {
    static_file_directory_ = directory;
}

std::string &HttpServer::getStaticFileDirectory() {
    return static_file_directory_;
}

void HttpServer::enableCORS(const std::string& origin) {
    cors_enabled_ = true;
    cors_origin_ = origin;
}

bool HttpServer::isCORSEnabled() const {
    return cors_enabled_;
}

std::string HttpServer::getCORSOrigin() const {
    return cors_origin_;
}

// 路由表管理辅助方法实现
void HttpServer::addRouteToTable(const std::string& method, const std::string& path, HttpRequestHandler handler) {
    // 按HTTP方法分组，直接在对应的unordered_map中插入路径-处理器映射
    route_tables_[method][path] = std::move(handler);
}

HttpRequestHandler HttpServer::findHandlerInTable(const std::string& method, const std::string& path) {
    // 第一层：按方法查找
    auto method_it = route_tables_.find(method);
    if (method_it == route_tables_.end()) {
        return nullptr; // 方法未找到
    }
    
    // 第二层：按路径查找，实现O(1)查找时间复杂度
    const auto& path_handler_map = method_it->second;
    auto path_it = path_handler_map.find(path);
    if (path_it == path_handler_map.end()) {
        return nullptr; // 路径未找到
    }
    
    return path_it->second;
}

void HttpServer::setMessageHandler(Connection::MessageHandler handler) {
    message_handler_ = std::move(handler);
}

void HttpServer::setStateChangeHandler(Connection::StateChangeHandler handler) {
    state_change_handler_ = std::move(handler);
}

void HttpServer::setCloseHandler(Connection::CloseHandler handler) {
    close_handler_ = std::move(handler);
}

void HttpServer::doAccept() {
    if (!running_) return;

    acceptor_.async_accept(
        [this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
            if (ec) {
                if (running_) {
                    std::cerr << "Accept error: " << ec.message() << std::endl;
                }
            } else {
                // 使用工具层的IdGenerator生成全局唯一连接ID
                auto connection_id = imserver::tool::IdGenerator::getInstance().generateConnectionId();
                
                // 创建新的HTTP会话
                auto session = std::make_shared<HttpConnection>(connection_id, std::move(socket), this);
                
                // 添加到连接管理器
                connection_manager_.addConnection(session);

                // 设置连接回调
                session->setMessageHandler(message_handler_);
                session->setStateChangeHandler(state_change_handler_);
                session->setCloseHandler(close_handler_);

                std::cout << "HTTP connection " << connection_id << " established" << std::endl;
                session->start();
            }

            // 继续接受下一个连接
            if (running_) {
                doAccept();
            }
        });
}

} // namespace network