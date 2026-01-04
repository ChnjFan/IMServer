#pragma once

#include <memory>
#include <vector>
#include <functional>
#include <future>
#include <atomic>
#include <queue>
#include <condition_variable>
#include <mutex>

#include <boost/asio.hpp>

namespace protocol {

/**
 * @brief 异步执行器类
 * 管理多个线程池，负责异步执行任务
 */
class AsyncExecutor {
public:
    /**
     * @brief 线程池类型
     */
    enum class PoolType {
        IO,     // IO密集型线程池
        CPU     // CPU密集型线程池
    };
    
    /**
     * @brief 构造函数
     * @param io_threads IO线程池大小，默认使用硬件线程数
     * @param cpu_threads CPU线程池大小，默认使用硬件线程数
     */
    explicit AsyncExecutor(size_t io_threads = 0, size_t cpu_threads = 0);
    
    /**
     * @brief 析构函数
     */
    ~AsyncExecutor();
    
    /**
     * @brief 提交异步任务
     * @tparam Func 任务函数类型
     * @tparam Args 任务参数类型
     * @param func 任务函数
     * @param args 任务参数
     * @return std::future<typename std::result_of<Func(Args...)>::type> 任务的future对象
     */
    template<typename Func, typename... Args>
    auto submit(Func&& func, Args&&... args) {
        return submitImpl<Func, Args...>(PoolType::CPU, std::forward<Func>(func), std::forward<Args>(args)...);
    }
    
    /**
     * @brief 提交IO密集型任务
     * @tparam Func 任务函数类型
     * @tparam Args 任务参数类型
     * @param func 任务函数
     * @param args 任务参数
     * @return std::future<typename std::result_of<Func(Args...)>::type> 任务的future对象
     */
    template<typename Func, typename... Args>
    auto submitIO(Func&& func, Args&&... args) {
        return submitImpl<Func, Args...>(PoolType::IO, std::forward<Func>(func), std::forward<Args>(args)...);
    }
    
    /**
     * @brief 提交CPU密集型任务
     * @tparam Func 任务函数类型
     * @tparam Args 任务参数类型
     * @param func 任务函数
     * @param args 任务参数
     * @return std::future<typename std::result_of<Func(Args...)>::type> 任务的future对象
     */
    template<typename Func, typename... Args>
    auto submitCPU(Func&& func, Args&&... args) {
        return submitImpl<Func, Args...>(PoolType::CPU, std::forward<Func>(func), std::forward<Args>(args)...);
    }
    
    /**
     * @brief 停止执行器
     */
    void stop();
    
    /**
     * @brief 等待所有任务完成
     */
    void wait();
    
    /**
     * @brief 设置线程池大小
     * @param pool_type 线程池类型
     * @param size 线程池大小
     */
    void setThreadPoolSize(PoolType pool_type, size_t size);
    
    /**
     * @brief 获取线程池大小
     * @param pool_type 线程池类型
     * @return size_t 线程池大小
     */
    size_t getThreadPoolSize(PoolType pool_type) const;
    
private:
    /**
     * @brief 任务包装器
     */
    using Task = std::function<void()>;
    
    /**
     * @brief 内部提交函数
     * @tparam Func 任务函数类型
     * @tparam Args 任务参数类型
     * @param pool_type 线程池类型
     * @param func 任务函数
     * @param args 任务参数
     * @return std::future<typename std::result_of<Func(Args...)>::type> 任务的future对象
     */
    template<typename Func, typename... Args>
    auto submitImpl(PoolType pool_type, Func&& func, Args&&... args) {
        using ReturnType = typename std::result_of<Func(Args...)>::type;
        
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
        
        std::future<ReturnType> result = task->get_future();
        
        // 提交任务到对应的线程池
        boost::asio::post(getThreadPool(pool_type), [task]() {
            (*task)();
        });
        
        return result;
    }
    
    /**
     * @brief 获取对应的线程池
     * @param pool_type 线程池类型
     * @return boost::asio::thread_pool& 线程池引用
     */
    boost::asio::thread_pool& getThreadPool(PoolType pool_type);
    
    /**
     * @brief 获取对应的线程池（const版本）
     * @param pool_type 线程池类型
     * @return const boost::asio::thread_pool& 线程池引用
     */
    const boost::asio::thread_pool& getThreadPool(PoolType pool_type) const;
    
    // 线程池
    std::unique_ptr<boost::asio::thread_pool> io_thread_pool_;     // IO密集型线程池
    std::unique_ptr<boost::asio::thread_pool> cpu_thread_pool_;   // CPU密集型线程池
    std::unordered_map<PoolType, size_t> thread_pool_sizes_;      // 线程池大小映射
    
    std::atomic<bool> running_;    // 执行器运行状态
};

} // namespace protocol
