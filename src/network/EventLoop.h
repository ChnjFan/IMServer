#pragma once

#include <boost/asio.hpp>
#include <thread>
#include <atomic>
#include <memory>

/**
 * Construct an EventLoop.
 *
 * Initializes internal io_context, work guard, worker thread state, and running flag.
 */

/**
 * Destroy the EventLoop.
 *
 * Stops the loop if running and joins the worker thread, releasing resources.
 */

/**
 * Start the event loop.
 *
 * Creates a work guard and launches the worker thread that runs the internal io_context.
 */

/**
 * Stop the event loop.
 *
 * Signals the io_context to stop, resets the work guard, and joins the worker thread.
 */

/**
 * Obtain the internal io_context.
 *
 * @returns Reference to the internal boost::asio::io_context used to schedule and run tasks.
 */

/**
 * Query whether the event loop is running.
 *
 * @returns `true` if the event loop's worker thread is active and the loop is running, `false` otherwise.
 */

/**
 * Schedule a handler to be executed by the event loop.
 *
 * @param handler Callable to be posted and invoked by the internal io_context.
 */

/**
 * Schedule a handler to run after a specified duration.
 *
 * The timer is kept alive until the handler executes. The handler is invoked only if the timer expires
 * without error.
 *
 * @tparam Duration A duration type convertible to boost::asio::steady_timer::duration (e.g., std::chrono types).
 * @param duration Time to wait before invoking the handler.
 * @param handler Callable invoked when the timer expires.
 */
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