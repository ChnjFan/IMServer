/**
 * @file Connection.h
 * @brief 连接管理类，用于管理单个客户端连接
 * 
 * Connection类基于boost::asio::ip::tcp::socket，支持异步读写操作，
 * 自动处理连接生命周期和缓冲区管理。
 */
#ifndef CONNECTION_H
#define CONNECTION_H

#include "EventSystem.h"
#include <boost/asio.hpp>
#include <memory>
#include <array>
#include <queue>
#include <functional>

namespace im {
namespace network {

class ConnectionEvent : public Event {
public:
    ConnectionEvent(boost::asio::ip::tcp::socket socket) : socket_(std::move(socket)) {}

    boost::asio::ip::tcp::socket& getSocket() { return socket_; }   

    std::type_index getType() const override { return typeid(ConnectionEvent); }
    std::string getName() const override { return "ConnectionEvent"; }

private:
    boost::asio::ip::tcp::socket socket_;
};

class Connection : public std::enable_shared_from_this<Connection> {
public:
    Connection();
    
    void send(const std::vector<char>& data);
    void close();
    
    boost::asio::ip::tcp::endpoint getRemoteEndpoint() const;

private:
    void startRead();
    void handleRead(const boost::system::error_code& ec, size_t bytes_transferred);
    void startWrite();
    void handleWrite(const boost::system::error_code& ec, size_t bytes_transferred);

    size_t connectionListenerId_;
};

} // namespace network
} // namespace im

#endif // CONNECTION_H