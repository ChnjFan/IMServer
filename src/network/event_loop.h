#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include <boost/asio/io_context.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <vector>
#include <thread>
#include <mutex>
#include <functional>
#include <atomic>

namespace im {
namespace network {

/**
 * @class Event
 * @brief 事件基类，所有事件都必须继承自该类
 * 
 * 事件类定义了事件的基本接口，所有具体的事件类型都必须实现handle()方法。
 * 事件循环会调用事件的handle()方法来处理事件。
 */ 
class Event {
public:
    virtual ~Event() = default;

    // 获取事件类型的唯一标识
    // std::type_index 类型索引工具，将 std::type_info 封装为可哈希的类型，让事件类型可以作为键值保存在 std::unordered_map 中
    virtual std::type_index getType() const = 0;
    // 获取事件名称（用于调试）
    virtual std::string getName() const = 0;

    // 事件优先级（数值越大优先级越高）
    int priority = 0;
    // 事件时间戳
    std::chrono::steady_clock::time_point timestamp;

    Event() : timestamp(std::chrono::steady_clock::now()) {}
};



/**
 * @class EventLoop
 * @brief 事件循环类，基于boost::asio::io_context实现，提供线程池支持
 * 
 * EventLoop是网络模块的核心组件，负责管理和调度网络事件、定时器、异步任务等。
 * 它内部维护一个线程池，通过boost::asio::io_context实现高效的事件驱动机制。
 * 
 * 注意：本IMServer采用多进程架构，每个模块作为单独进程运行，EventLoop需要与其他进程通信。
 */
class EventLoop {
public:
    /**
     * @brief 构造函数
     * @param thread_pool_size 线程池大小，默认为1
     */
    explicit EventLoop(size_t thread_pool_size = 1);

    /**
     * @brief 析构函数
     */
    ~EventLoop();

    /**
     * @brief 启动事件循环
     * 
     * 启动线程池，开始处理事件队列中的任务
     */
    void start();

    /**
     * @brief 停止事件循环
     * 
     * 停止所有线程，不再处理新的事件
     */
    void stop();

    /**
     * @brief 将任务提交到事件循环（异步执行，不阻塞）
     * @tparam Func 任务函数类型
     * @param func 任务函数
     */
    template<typename Func>
    void post(Func&& func) {
        io_context_.post(std::forward<Func>(func));
    }

    /**
     * @brief 将任务提交到事件循环（如果在事件循环线程中则直接执行，否则异步执行）
     * @tparam Func 任务函数类型
     * @param func 任务函数
     */
    template<typename Func>
    void dispatch(Func&& func) {
        io_context_.dispatch(std::forward<Func>(func));
    }

    /**
     * @brief 获取io_context
     * @return boost::asio::io_context& io_context引用
     */
    boost::asio::io_context& getIOContext();

    /**
     * @brief 获取线程池大小
     * @return 线程池大小
     */
    size_t getThreadPoolSize() const;

    /**
     * @brief 检查事件循环是否正在运行
     * @return bool 是否正在运行
     */
    bool isRunning() const;

private:
    boost::asio::io_context io_context_;  ///< boost::asio事件上下文
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;  ///< 工作守卫，防止io_context自动退出
    std::vector<std::thread> threads_;  ///< 线程池
    size_t thread_pool_size_;  ///< 线程池大小
    std::atomic<bool> running_;  ///< 运行状态标志
    std::mutex mutex_;  ///< 互斥锁，用于保护共享数据

    /**
     * @brief 线程函数，运行io_context.run()
     */
    void threadFunc();
};

} // namespace network
} // namespace im

#endif // EVENT_LOOP_H
