#include "TcpServer.h"
#include "IdGenerator.h"
#include <iostream>

namespace network {

TcpConnection::TcpConnection(ConnectionId id, ip::tcp::socket socket)
    : network::Connection(id, ConnectionType::TCP)
    , socket_(std::move(socket))
    , read_buffer_(4096)
    , running_(false) {
}

TcpConnection::~TcpConnection() {
    close();
}

void TcpConnection::start() {
    if (!running_) {
        running_ = true;
        // 连接成功后，设置状态为Connecting，等待校验Token
        setState(ConnectionState::Connecting);
        doRead();
    }
}

void TcpConnection::close() {
    if (running_) {
        running_ = false;
        boost::system::error_code ec;
        socket_.close(ec);
        if (close_handler_) {
            close_handler_(connection_id_, ec);
        }
    }
}

void TcpConnection::send(const std::vector<char> &data)
{
    if (!running_) return;

    bool write_in_progress = false;
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        // 如果当前有正在进行的写入操作，直接将数据加入写入缓冲区
        write_in_progress = !write_buffer_.empty();
        write_buffer_.insert(write_buffer_.end(), data.begin(), data.end());
    }
    
    if (!write_in_progress) {
        auto self = shared_from_this();
        asio::async_write(socket_, asio::buffer(write_buffer_),
            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    std::lock_guard<std::mutex> lock(write_mutex_);
                    write_buffer_.clear();
                } else {
                    close();
                }
            });
    }
}

// 实现缺失的send方法：发送字符串数据
void TcpConnection::send(const std::string& data)
{
    std::vector<char> char_data(data.begin(), data.end());
    send(char_data);
}

// 实现缺失的send方法：发送右值引用数据
void TcpConnection::send(std::vector<char>&& data)
{
    if (!running_) return;

    bool write_in_progress = false;
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        // 如果当前有正在进行的写入操作，将数据加入写入缓冲区
        write_in_progress = !write_buffer_.empty();
        if (write_in_progress) {
            write_buffer_.insert(write_buffer_.end(), data.begin(), data.end());
        } else {
            // 没有正在进行的写入操作，直接使用传入的数据
            write_buffer_.swap(data);
        }
    }
    
    if (!write_in_progress) {
        auto self = shared_from_this();
        asio::async_write(socket_, asio::buffer(write_buffer_),
            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    std::lock_guard<std::mutex> lock(write_mutex_);
                    write_buffer_.clear();
                } else {
                    close();
                }
            });
    }
}

// 实现缺失的forceClose方法
void TcpConnection::forceClose()
{
    running_ = false;
    socket_.close();
    setState(ConnectionState::Disconnected);
}

ip::tcp::endpoint TcpConnection::getRemoteEndpoint() const
{
    boost::system::error_code ec;
    auto endpoint = socket_.remote_endpoint(ec);
    if (ec) {
        return ip::tcp::endpoint();
    }
    return endpoint;
}

std::string TcpConnection::getRemoteAddress() const
{
    boost::system::error_code ec;
    auto endpoint = socket_.remote_endpoint(ec);
    if (ec) {
        return "";
    }
    return endpoint.address().to_string();
}

uint16_t TcpConnection::getRemotePort() const
{
    boost::system::error_code ec;
    auto endpoint = socket_.remote_endpoint(ec);
    if (ec) {
        return 0;
    }
    return endpoint.port();
}

bool TcpConnection::isConnected() const
{
    return socket_.is_open();
}

void TcpConnection::doRead()
{
    auto self = shared_from_this();
    socket_.async_read_some(asio::buffer(read_buffer_),
        [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
            updateBytesReceived(bytes_transferred);
            incrementMessagesReceived();
            if (ec) {
                close();
                return;
            }
            try {
                triggerMessageHandler(read_buffer_);
                read_buffer_.erase(read_buffer_.begin(), read_buffer_.begin() + bytes_transferred);
                if (read_buffer_.empty()) {
                    read_buffer_.resize(4096);
                }
                //todo 怎么校验Token？
                
                doRead();
            }
            catch(const std::exception& e) {
                std::cerr << "Error in TcpConnection::doRead: " << e.what() << '\n';
                close();
            }
        });
}

TcpServer::TcpServer(asio::io_context& io_context, ConnectionManager& connection_manager, const std::string& address, uint16_t port)
    : io_context_(io_context),
      acceptor_(io_context, ip::tcp::endpoint(ip::address::from_string(address), port)),
      connection_manager_(connection_manager),
      running_(false) {
}

TcpServer::~TcpServer() {
    stop();
}

void TcpServer::start() {
    if (!running_) {
        running_ = true;
        doAccept();
    }
}

void TcpServer::stop() {
    if (running_) {
        running_ = false;
        boost::system::error_code ec;
        acceptor_.close(ec);

        connection_manager_.closeConnectionsByType(ConnectionType::TCP);
    }
}

bool TcpServer::isRunning() const {
    return running_;
}

void TcpServer::setMessageHandler(Connection::MessageHandler handler)
{
    message_handler_ = handler;
}

void TcpServer::setStateChangeHandler(Connection::StateChangeHandler handler)
{
    state_change_handler_ = handler;
}

void TcpServer::setCloseHandler(Connection::CloseHandler handler)
{
    close_handler_ = handler;
}

void TcpServer::doAccept() {
    if (!running_) return;
    auto self = shared_from_this();
    acceptor_.async_accept(
        [self](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
            if (!ec && self->running_) {
                try {
                    self->handleAccept(ec, std::move(socket));
                    self->doAccept();
                }
                catch(const std::exception& e) {
                    std::cerr << "Error in TcpServer::async_accept callback: " << e.what() << std::endl;
                }
                catch(...) {
                    std::cerr << "Unknown error in TcpServer::async_accept callback" << std::endl;
                }
            }
        });
}

void TcpServer::handleAccept(boost::system::error_code ec, ip::tcp::socket socket) {
    if (!ec) {
        try {
            auto connection_id = imserver::tool::IdGenerator::getInstance().generateConnectionId();
            auto conn = std::make_shared<TcpConnection>(connection_id, std::move(socket));

            // TCP连接后立即接收数据，校验Token才能建立连接
            conn->setMessageHandler(message_handler_);
            conn->setStateChangeHandler(state_change_handler_);
            conn->setCloseHandler(close_handler_);

            connection_manager_.addConnection(conn);
            conn->start();
        }
        catch(const std::exception& e) {
            std::cerr << "Error in TcpServer::handleAccept: " << e.what() << std::endl;
            // 捕获异常后，继续接受新连接，避免服务器崩溃
            if (running_) {
                doAccept();
            }
        }
        catch(...) {
            std::cerr << "Unknown error in TcpServer::handleAccept" << std::endl;
            // 捕获未知异常后，继续接受新连接
            if (running_) {
                doAccept();
            }
        }
    }
}

} // namespace network
