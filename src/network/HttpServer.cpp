#include "HttpServer.h"
#include <iostream>
#include <regex>
#include <sstream>
#include <algorithm>

namespace network {

HttpServer::HttpServer(boost::asio::io_context& io_context, const std::string& address, uint16_t port)
    : io_context_(io_context),
      acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address(address), port)),
      running_(false) {
}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::start() {
    if (running_.exchange(true)) {
        return; // 已经在运行
    }
    
    // 开始接受连接
    doAccept();
    std::cout << "HTTP Server started on " << acceptor_.local_endpoint().address().to_string() 
              << ":" << acceptor_.local_endpoint().port() << std::endl;
}

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

bool HttpServer::isRunning() const {
    return running_;
}

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

void HttpServer::removeConnection(Connection::Ptr conn) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_.erase(conn);
}

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

void HttpServer::enableHttps(const std::string& cert_file, const std::string& private_key_file) {
    // TODO: 实现HTTPS支持
    std::cerr << "HTTPS support not implemented yet" << std::endl;
}

} // namespace network