#pragma once

#include "ConnectionManager.h"
#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <string>
#include <unordered_set>
#include <mutex>
#include <atomic>

namespace network {

// 添加与其他服务器一致的命名空间别名
namespace asio = boost::asio;
namespace ip = asio::ip;

class TcpConnection : public Connection {
public:
    using Ptr = std::shared_ptr<TcpConnection>;

private:
    ip::tcp::socket socket_;
    std::vector<char> read_buffer_;
    std::vector<char> write_buffer_;
    std::mutex write_mutex_;
    std::atomic<bool> running_;

public:
    TcpConnection(ConnectionId id, ip::tcp::socket socket);
    ~TcpConnection();
    
    void start() override;
    void close() override;
    void send(const std::vector<char>& data) override;
    
    ip::tcp::endpoint getRemoteEndpoint() const override;
    std::string getRemoteAddress() const override;
    uint16_t getRemotePort() const override;
    bool isConnected() const override;

private:
    void doRead();
};

class TcpServer : public std::enable_shared_from_this<TcpServer> {
private:
    asio::io_context& io_context_;
    ip::tcp::acceptor acceptor_;
    ConnectionManager& connection_manager_;
    std::atomic<bool> running_;

public:
    TcpServer(asio::io_context& io_context, ConnectionManager& connection_manager, const std::string& address, uint16_t port);
    ~TcpServer();
    
    void start();
    void stop();
    bool isRunning() const;
    
private:
    void doAccept();
    void handleAccept(boost::system::error_code ec, ip::tcp::socket socket);
};

} // namespace network
