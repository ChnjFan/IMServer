#include "WebSocketServer.h"
#include <iostream>
#include <memory>

namespace network {

// WebSocketSession实现
WebSocketSession::WebSocketSession(asio::ip::tcp::socket socket)
    : ws_(std::move(socket)) {
    // 设置WebSocket选项
    ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
    ws_.set_option(websocket::stream_base::decorator(
        [](websocket::response_type& res) {
            res.set(http::field::server, std::string(BOOST_BEAST_VERSION_STRING) + " WebSocketServer");
        }));
}

WebSocketSession::~WebSocketSession() {
    if (ws_.is_open()) {
        beast::error_code ec;
        ws_.close(websocket::close_code::normal, ec);
    }
}

void WebSocketSession::start() {
    // 执行WebSocket握手
    ws_.async_accept(
        beast::bind_front_handler(&WebSocketSession::onAccept, shared_from_this()));
}

void WebSocketSession::sendText(const std::string& message) {
    send(std::vector<char>(message.begin(), message.end()));
}

void WebSocketSession::sendBinary(const std::vector<char>& data) {
    send(data);
}

void WebSocketSession::send(const std::vector<char>& data) {
    // 检查是否正在写入，如果是则加入队列
    if (!write_queue_.empty()) {
        write_queue_.push(data);
        return;
    }

    // 开始异步写入
    doWrite(data);
}

void WebSocketSession::close() {
    doClose();
}

void WebSocketSession::setMessageHandler(MessageHandler handler) {
    message_handler_ = handler;
}

void WebSocketSession::setCloseHandler(CloseHandler handler) {
    close_handler_ = handler;
}

void WebSocketSession::doRead() {
    // 读取下一条消息
    ws_.async_read(
        buffer_,
        beast::bind_front_handler(&WebSocketSession::onRead, shared_from_this()));
}

void WebSocketSession::onRead(beast::error_code ec, std::size_t bytes_transferred) {
    if (ec) {
        if (ec == websocket::error::closed) {
            // 连接正常关闭
            if (close_handler_) {
                close_handler_(shared_from_this());
            }
        } else {
            std::cerr << "WebSocket read error: " << ec.message() << std::endl;
            doClose();
        }
        return;
    }

    // 获取消息数据
    std::vector<char> data(beast::buffers_cast<char*>(buffer_.data()), 
                          beast::buffers_cast<char*>(buffer_.data()) + buffer_.size());

    // 清空缓冲区
    buffer_.consume(buffer_.size());

    // 调用消息处理器
    if (message_handler_) {
        message_handler_(shared_from_this(), data);
    }

    // 继续读取下一条消息
    doRead();
}

void WebSocketSession::doWrite(const std::vector<char>& data) {
    // 创建写操作
    ws_.async_write(
        boost::asio::buffer(data),
        beast::bind_front_handler(&WebSocketSession::onWrite, shared_from_this(), data));
}

void WebSocketSession::onWrite(beast::error_code ec, std::size_t bytes_transferred, std::vector<char> data) {
    if (ec) {
        std::cerr << "WebSocket write error: " << ec.message() << std::endl;
        doClose();
        return;
    }

    // 检查是否有更多消息需要发送
    if (!write_queue_.empty()) {
        auto next_data = std::move(write_queue_.front());
        write_queue_.pop();
        doWrite(std::move(next_data));
    }
}

void WebSocketSession::doClose() {
    if (ws_.is_open()) {
        ws_.async_close(websocket::close_code::normal,
                       beast::bind_front_handler(&WebSocketSession::onClose, shared_from_this()));
    }
}

void WebSocketSession::onClose(beast::error_code ec) {
    if (ec && ec != websocket::error::closed) {
        std::cerr << "WebSocket close error: " << ec.message() << std::endl;
    }
    
    if (close_handler_) {
        close_handler_(shared_from_this());
    }
}

void WebSocketSession::onAccept(beast::error_code ec) {
    if (ec) {
        std::cerr << "WebSocket accept error: " << ec.message() << std::endl;
        doClose();
        return;
    }

    // 握手成功，开始读取消息
    doRead();
}

// WebSocketServer实现
WebSocketServer::WebSocketServer(asio::io_context& io_context, const std::string& address, uint16_t port)
    : io_context_(io_context),
      acceptor_(io_context, asio::ip::tcp::endpoint(asio::ip::make_address(address), port)),
      running_(false) {
}

WebSocketServer::~WebSocketServer() {
    stop();
}

void WebSocketServer::start() {
    if (!running_) {
        running_ = true;
        doAccept();
    }
}

void WebSocketServer::stop() {
    if (running_) {
        running_ = false;
        
        // 关闭acceptor
        beast::error_code ec;
        acceptor_.close(ec);
        
        // 关闭所有连接
        std::lock_guard<std::mutex> lock(connections_mutex_);
        for (auto& conn : connections_) {
            conn->close();
        }
        connections_.clear();
    }
}

bool WebSocketServer::isRunning() const {
    return running_;
}

void WebSocketServer::broadcastText(const std::string& message) {
    auto data = std::vector<char>(message.begin(), message.end());
    broadcastBinary(data);
}

void WebSocketServer::broadcastBinary(const std::vector<char>& data) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    // 遍历所有连接，发送消息
    for (auto it = connections_.begin(); it != connections_.end();) {
        auto session = *it;
        if (session && session->is_open()) {
            session->send(data);
            ++it;
        } else {
            // 移除无效连接
            it = connections_.erase(it);
        }
    }
}

void WebSocketServer::doAccept() {
    if (!running_) return;

    // 异步接受新连接
    acceptor_.async_accept(
        beast::bind_front_handler(&WebSocketServer::onAccept, shared_from_this()));
}

void WebSocketServer::onAccept(beast::error_code ec, asio::ip::tcp::socket socket) {
    if (ec) {
        std::cerr << "Accept error: " << ec.message() << std::endl;
    } else {
        // 创建新的WebSocket会话
        auto session = std::make_shared<WebSocketSession>(std::move(socket));
        
        // 设置消息处理器（示例：简单回显）
        session->setMessageHandler([this](WebSocketSession::Ptr session, const std::vector<char>& data) {
            // 处理接收到的消息
            std::cout << "Received message: " << std::string(data.begin(), data.end()) << std::endl;
            
            // 回显消息
            session->send(data);
        });
        
        // 设置关闭处理器
        session->setCloseHandler([this](WebSocketSession::Ptr session) {
            removeConnection(session);
        });
        
        // 添加到连接集合
        addConnection(session);
        
        // 启动会话
        session->start();
    }

    // 继续接受下一个连接
    if (running_) {
        doAccept();
    }
}

void WebSocketServer::addConnection(WebSocketSession::Ptr session) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_.insert(session);
}

void WebSocketServer::removeConnection(WebSocketSession::Ptr session) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_.erase(session);
}

} // namespace network