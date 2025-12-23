#pragma once

#include <boost/asio.hpp>
#include <thread>
#include <atomic>
#include <memory>

namespace network {

class EventLoop {
private:
    boost::asio::io_context io_context_;
    std::unique_ptr<boost::asio::io_context::work> work_;
    std::thread thread_;
    std::atomic<bool> running_;

public:
    EventLoop();
    ~EventLoop();
    
    // 启动事件循环
    void start();
    
    // 停止事件循环
    void stop();
    
    // 获取io_context对象
    boost::asio::io_context& getIoContext();
    
    // 运行状态检查
    bool isRunning() const;
    
    // 延迟执行任务
    template<typename CompletionHandler>
    void post(CompletionHandler handler) {
        io_context_.post(handler);
    }
    
    // 定时执行任务
    template<typename Duration, typename CompletionHandler>
    void runAfter(Duration duration, CompletionHandler handler) {
        auto timer = std::make_shared<boost::asio::steady_timer>(io_context_, duration);
        timer->async_wait([timer, handler](const boost::system::error_code& ec) {
            if (!ec) {
                handler();
            }
        });
    }
};

} // namespace network
