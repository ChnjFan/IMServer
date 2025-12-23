#pragma once

#include <boost/asio.hpp>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <atomic>

namespace network {

class Connection : public std::enable_shared_from_this<Connection> {
public:
    using Ptr = std::shared_ptr<Connection>;
    using MessageHandler = std::function<void(const Ptr&, const std::vector<char>&)>;
    using CloseHandler = std::function<void(const Ptr&)>;
    
private:
    boost::asio::ip::tcp::socket socket_;
    std::vector<char> read_buffer_;
    std::vector<char> write_buffer_;
    std::mutex write_mutex_;
    MessageHandler message_handler_;
    CloseHandler close_handler_;
    std::atomic<bool> closed_;

public:
    explicit Connection(boost::asio::ip::tcp::socket socket);
    ~Connection();
    
    // 启动连接
    void start();
    
    // 关闭连接
    void close();
    
    // 发送数据
    void send(const std::vector<char>& data);
    
    // 获取远程端点信息
    boost::asio::ip::tcp::endpoint getRemoteEndpoint() const;
    
    // 设置消息处理回调
    void setMessageHandler(MessageHandler handler);
    
    // 设置关闭处理回调
    void setCloseHandler(CloseHandler handler);
    
    // 连接状态检查
    bool isClosed() const;
    
private:
    // 异步读取数据
    void doRead();
    
    // 异步写入数据
    void doWrite();
};

} // namespace network
