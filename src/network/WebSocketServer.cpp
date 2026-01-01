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
    ws_.async_accept(
        [self = shared_from_this()](beast::error_code ec) {
            if (ec) {
                std::cerr << "WebSocket accept error: " << ec.message() << std::endl;
                self->close();
                return;
            }

            running_ = true;
            self->doRead();
        });
}

void WebSocketConnection::close() {
    if (ws_.next_layer().is_open() && running_) {
        running_ = false;
        ws_.async_close(websocket::close_code::normal,
            [self = shared_from_this()](beast::error_code ec) {
                if (ec && ec != websocket::error::closed) {
                    std::cerr << "WebSocket close error: " << ec.message() << std::endl;
                }
                
                if (self->close_handler_) {
                    self->close_handler_(self->getId(), ec);
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

void WebSocketConnection::doRead() {
    ws_.async_read(
        buffer_,
        [self = shared_from_this(), &buffer = buffer_](beast::error_code ec, std::size_t bytes_transferred) {
            incrementMessagesReceived();
            updateBytesReceived(bytes_transferred);
            if (ec) {
                if (ec == websocket::error::closed) {
                    if (self->close_handler_) {
                        self->close_handler_(self);
                    }
                } else {
                    std::cerr << "WebSocket read error: " << ec.message() << std::endl;
                    self->doClose();
                }
                return;
            }

            std::vector<char> data(
                boost::asio::buffer_cast<const char*>(buffer.data()),
                boost::asio::buffer_cast<const char*>(buffer.data()) + buffer.size());

            buffer.consume(triggerMessageHandler(data));
            self->doRead();
        });
}

void WebSocketConnection::doWrite(std::vector<char>&& data) {
    ws_.async_write(
        boost::asio::buffer(data),
        [self = shared_from_this(), data = std::move(data)](beast::error_code ec, std::size_t bytes_transferred) {
            if (ec) {
                std::cerr << "WebSocket write error: " << ec.message() << std::endl;
                self->close();
                return;
            }

            // 检查是否有更多消息需要发送
            if (!self->write_queue_.empty()) {
                auto next_data = std::move(self->write_queue_.front());
                self->write_queue_.pop();
                self->doWrite(std::move(next_data));
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

void WebSocketServer::doAccept() {
    if (!running_) return;
    acceptor_.async_accept(
        [self = shared_from_this()](beast::error_code ec, asio::ip::tcp::socket socket) {
            if (!ec && self->running_) {
                self->handleAccept(ec, std::move(socket));
                self->doAccept();
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
        // todo 建立连接后，从请求中获取token直接校验

        auto connection_id = imserver::tool::IdGenerator::getInstance().generateConnectionId();
        auto conn = std::make_shared<WebSocketConnection>(connection_id, std::move(socket));

        conn->setMessageHandler([this](ConnectionId conn_id, const std::vector<char>& data) {
            //todo 处理消息
            std::cout << "Received message from WebSocket connection " << conn_id << ": " 
                        << std::string(data.begin(), data.end()) << std::endl;
            return data.size();
        });

        conn->setStateChangeHandler([this](ConnectionId conn_id, ConnectionState old_state, ConnectionState new_state) {
            std::cout << "Connection " << conn_id << " state changed from " << old_state << " to " << new_state << std::endl;
        });
        
        conn->setCloseHandler([this](ConnectionId conn_id, const boost::system::error_code& ec) {
            std::cout << "WebSocket connection " << conn_id << " closed: " << ec.message() << std::endl;
        });

        connection_manager_.addConnection(conn);
        conn->start();
    }
}

} // namespace network