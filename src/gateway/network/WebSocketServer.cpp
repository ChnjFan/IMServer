#include "WebSocketServer.h"
#include "IdGenerator.h"
#include <iostream>
#include <memory>

namespace network {

// WebSocketSession实现
WebSocketConnection::WebSocketConnection(ConnectionId id, asio::ip::tcp::socket socket)
    : Connection(id, ConnectionType::WebSocket), ws_(std::move(socket)) {
    // 设置WebSocket选项
    ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
    ws_.set_option(websocket::stream_base::decorator(
        [](websocket::response_type& res) {
            res.set(http::field::server, std::string(BOOST_BEAST_VERSION_STRING) + " WebSocketServer");
        }));
}

// 在WebSocketSession析构函数中
WebSocketConnection::~WebSocketConnection() {
    forceClose();// 析构函数强制关闭连接
}

void WebSocketConnection::start() {
    // 执行WebSocket握手
    auto self = shared_from_this();
    ws_.async_accept(
        [this, self](beast::error_code ec) {
            if (ec) {
                std::cerr << "WebSocket accept error: " << ec.message() << std::endl;
                close();
                return;
            }

            running_ = true;
            doRead();
        });
}

void WebSocketConnection::close() {
    if (ws_.next_layer().is_open() && running_) {
        running_ = false;
        auto self = shared_from_this();
        ws_.async_close(websocket::close_code::normal,
            [this, self](beast::error_code ec) {
                if (ec && ec != websocket::error::closed) {
                    std::cerr << "WebSocket close error: " << ec.message() << std::endl;
                }
                
                if (close_handler_) {
                    close_handler_(getId(), ec);
                }
            });
    }
}

void WebSocketConnection::forceClose()
{
    if (ws_.next_layer().is_open()) {
        running_ = false;
        beast::error_code ec;
        ws_.close(websocket::close_code::normal, ec);
    }
}

void WebSocketConnection::send(const std::vector<char>& data) {
    if (!running_) return;

    bool write_in_progress = false;
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        write_in_progress = !write_queue_.empty();
        if (write_in_progress) {    // 如果当前有正在进行的写入操作，直接将数据加入写入缓冲区
            write_queue_.push(data);
        }
    }

    if (!write_in_progress) {
        doWrite(data);
    }
}

void WebSocketConnection::send(const std::string& data) {
    std::vector<char> char_data(data.begin(), data.end());
    send(char_data);
}

void WebSocketConnection::send(std::vector<char>&& data) {
    if (!running_) return;

    bool write_in_progress = false;
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        write_in_progress = !write_queue_.empty();
        if (write_in_progress) {
            // 如果当前有正在进行的写入操作，将数据加入写入缓冲区
            write_queue_.push(std::move(data));
        }
    }

    if (!write_in_progress) {
        doWrite(data);
    }
}

boost::asio::ip::tcp::endpoint WebSocketConnection::getRemoteEndpoint() const
{
    boost::system::error_code ec;
    auto endpoint = ws_.next_layer().remote_endpoint(ec);
    if (ec) {
        return boost::asio::ip::tcp::endpoint();
    }
    return endpoint;
}

bool WebSocketConnection::isConnected() const
{
    return ws_.next_layer().is_open();
}

// 实现缺失的getRemoteAddress方法
std::string WebSocketConnection::getRemoteAddress() const {
    boost::system::error_code ec;
    auto endpoint = ws_.next_layer().remote_endpoint(ec);
    if (ec) {
        return "";
    }
    return endpoint.address().to_string();
}

// 实现缺失的getRemotePort方法
uint16_t WebSocketConnection::getRemotePort() const {
    boost::system::error_code ec;
    auto endpoint = ws_.next_layer().remote_endpoint(ec);
    if (ec) {
        return 0;
    }
    return endpoint.port();
}

void WebSocketConnection::doRead() {
    auto self = shared_from_this();
    ws_.async_read(
        buffer_,
        [this, self, &buffer = buffer_](beast::error_code ec, std::size_t bytes_transferred) {
            incrementMessagesReceived();
            updateBytesReceived(bytes_transferred);
            if (ec) {
                if (ec == websocket::error::closed) {
                    if (close_handler_) {
                        close_handler_(getId(), ec);
                    }
                } else {
                    std::cerr << "WebSocket read error: " << ec.message() << std::endl;
                    close();
                }
                return;
            }

            std::vector<char> data(
                boost::asio::buffer_cast<const char*>(buffer.data()),
                boost::asio::buffer_cast<const char*>(buffer.data()) + buffer.size());

            triggerMessageHandler(data);
            buffer.consume(bytes_transferred);
            doRead();
        });
}

void WebSocketConnection::doWrite(const std::vector<char>& data) {
    auto self = shared_from_this();
    ws_.async_write(
        boost::asio::buffer(data),
        [this, self, data = std::move(data)](beast::error_code ec, std::size_t /*bytes_transferred*/) {
            if (ec) {
                std::cerr << "WebSocket write error: " << ec.message() << std::endl;
                close();
                return;
            }

            // 检查是否有更多消息需要发送
            if (!write_queue_.empty()) {
                auto next_data = std::move(write_queue_.front());
                write_queue_.pop();
                doWrite(next_data);
            }
        });
}

// WebSocketServer实现
WebSocketServer::WebSocketServer(asio::io_context& io_context, ConnectionManager& connection_manager, const std::string& address, uint16_t port)
    : io_context_(io_context),
      acceptor_(io_context, asio::ip::tcp::endpoint(asio::ip::make_address(address), port)),
      connection_manager_(connection_manager),
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
        beast::error_code ec;
        acceptor_.close(ec);
        
        connection_manager_.closeConnectionsByType(ConnectionType::WebSocket);
    }
}

bool WebSocketServer::isRunning() const {
    return running_;
}

// 设置连接回调
void WebSocketServer::setMessageHandler(Connection::MessageHandler handler) {
    message_handler_ = std::move(handler);
}
void WebSocketServer::setStateChangeHandler(Connection::StateChangeHandler handler) {
    state_change_handler_ = std::move(handler);
}
void WebSocketServer::setCloseHandler(Connection::CloseHandler handler) {
    close_handler_ = std::move(handler);
}

void WebSocketServer::doAccept() {
    if (!running_) return;
    auto self = shared_from_this();
    acceptor_.async_accept(
        [self](beast::error_code ec, asio::ip::tcp::socket socket) {
            if (!ec && self->running_) {
                try {
                    self->handleAccept(ec, std::move(socket));
                    self->doAccept();
                }
                catch(const std::exception& e) {
                    std::cerr << "Error in WebSocketServer::async_accept callback: " << e.what() << std::endl;
                }
                catch(...) {
                    std::cerr << "Unknown error in WebSocketServer::async_accept callback" << std::endl;
                }
            }
        });
}

/**
 * @brief 处理新的WebSocket连接
 * 
 * 该函数在新的WebSocket连接被接受时被调用。它负责创建新的连接会话、分配连接ID、
 * 注册消息处理函数、状态变更处理函数和关闭处理函数，并将新连接添加到连接管理器中。
 * 
 * @param ec 异步操作的错误码
 * @param socket 新的TCP socket，用于与客户端通信
 */
void WebSocketServer::handleAccept(beast::error_code ec, asio::ip::tcp::socket socket) {
    if (ec) {
        std::cerr << "Accept error: " << ec.message() << std::endl;
    } else {
        try {
            // todo 建立连接后，从请求中获取token直接校验

            auto connection_id = imserver::tool::IdGenerator::getInstance().generateConnectionId();
            auto conn = std::make_shared<WebSocketConnection>(connection_id, std::move(socket));

            // 设置连接回调
            conn->setMessageHandler(message_handler_);
            conn->setStateChangeHandler(state_change_handler_);
            conn->setCloseHandler(close_handler_);

            connection_manager_.addConnection(conn);
            conn->start();
        }
        catch(const std::exception& e) {
            std::cerr << "Error in WebSocketServer::handleAccept: " << e.what() << std::endl;
        }
        catch(...) {
            std::cerr << "Unknown error in WebSocketServer::handleAccept" << std::endl;
        }
    }
}

} // namespace network