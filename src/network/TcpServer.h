/**
 * @file TcpServer.h
 * @brief TCP服务器类，用于监听TCP端口并接受客户端连接
 * 
 * TcpServer类基于boost::asio::ip::tcp::acceptor，支持异步接受连接，
 * 自动管理连接生命周期，并与EventSystem集成处理网络事件。
 */
#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <boost/asio.hpp>
#include <memory>
#include "EventSystem.h"
#include "Connection.h"

namespace im {
namespace network {

class TcpServer {
private:
    EventSystem& event_system_;
    boost::asio::ip::tcp::acceptor acceptor_;
    Connection::MessageHandler message_handler_;
    Connection::CloseHandler close_handler_;
    
    void startAccept();
    void handleAccept(boost::asio::ip::tcp::socket socket, const boost::system::error_code& ec);

public:
    TcpServer(EventSystem& event_system, const std::string& host, uint16_t port);
    
    void start();
    void stop();
    
    void setMessageHandler(Connection::MessageHandler handler);
    void setCloseHandler(Connection::CloseHandler handler);
};

} // namespace network
} // namespace im

#endif // TCP_SERVER_H