#include "TcpServer.h"
#include "IdGenerator.h"
#include <iostream>

namespace network {

TcpConnection::TcpConnection(ConnectionId id, boost::asio::ip::tcp::socket socket)
    : Connection(id, ConnectionType::TCP), socket_(std::move(socket)), read_buffer_(4096), running_(false) {
}

TcpConnection::~TcpConnection() {
    close();
}

void TcpConnection::start() {
    if (!running_) {
        running_ = true;
        doRead();
    }
}

void TcpConnection::close() {
    if (running_) {
        running_ = false;
        boost::system::error_code ec;
        socket_.close(ec);
        if (close_handler_) {
            close_handler_(id, ec);
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
        boost::asio::async_write(socket_, boost::asio::buffer(write_buffer_),
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

boost::asio::ip::tcp::endpoint TcpConnection::getRemoteEndpoint() const
{
    boost::system::error_code ec;
    auto endpoint = socket_.remote_endpoint(ec);
    if (ec) {
        return boost::asio::ip::tcp::endpoint();
    }
    return endpoint;
}

bool TcpConnection::isConnected() const
{
    return socket_.is_open();
}

void TcpConnection::doRead()
{
    auto self = shared_from_this();
    socket_.async_read_some(boost::asio::buffer(read_buffer_),
        [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
            incrementMessagesReceived();
            updateBytesReceived(bytes_transferred);
            if (ec) {
                close();
                return;
            }
            try {
                boost::asio::dynamic_buffer dyn_buffer(read_buffer_);
                std::vector<char> data(dyn_buffer.data().begin(), 
                                      dyn_buffer.data().end());
                
                dyn_buffer.consume(triggerMessageHandler(data));
                
                doRead();
            }
            catch(const std::exception& e) {
                std::cerr << "Error in TcpConnection::doRead: " << e.what() << '\n';
                close();
            }
        });
}

TcpServer::TcpServer(boost::asio::io_context& io_context, ConnectionManager& connection_manager, const std::string& address, uint16_t port)
    : io_context_(io_context),
      acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address(address), port)),
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

void TcpServer::doAccept() {
    if (!running_) return;
    acceptor_.async_accept(
        [self = shared_from_this()](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
            if (!ec && self->running_) {
                self->handleAccept(ec, std::move(socket));
                self->doAccept();
            }
        });
}

void TcpServer::handleAccept(boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
    if (!ec) {
        auto connection_id = imserver::tool::IdGenerator::getInstance().generateConnectionId();
        auto conn = std::make_shared<TcpConnection>(connection_id, std::move(socket));
        
        // TCP连接后立即接收数据，校验Token才能建立连接
        conn->setState(ConnectionState::Connecting);
        conn->setMessageHandler([this](ConnectionId conn_id, const std::vector<char>& data) {
            //todo 处理收到的消息
            std::cout << "Received message from connection " << conn_id << ": " 
                      << std::string(data.begin(), data.end()) << std::endl;
            return data.size();
        });

        conn->setStateChangeHandler([this](ConnectionId conn_id, ConnectionState old_state, ConnectionState new_state) {
            std::cout << "Connection " << conn_id << " state changed from " << old_state << " to " << new_state << std::endl;
        });
        
        conn->setCloseHandler([this](ConnectionId conn_id, const boost::system::error_code& ec) {
            std::cout << "Connection " << conn_id << " closed: " << ec.message() << std::endl;
        });
        
        connection_manager_.addConnection(conn);
        conn->start();
    }
}

} // namespace network
