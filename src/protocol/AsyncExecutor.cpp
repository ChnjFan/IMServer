#include "AsyncExecutor.h"

#include <thread>

namespace protocol {

AsyncExecutor::AsyncExecutor(size_t io_threads, size_t cpu_threads) : running_(true) {
    // 如果未指定线程数，使用硬件线程数
    if (io_threads == 0) {
        io_threads = std::thread::hardware_concurrency();
    }
    
    if (cpu_threads == 0) {
        cpu_threads = std::thread::hardware_concurrency();
    }
    
    // 创建线程池
    io_thread_pool_ = std::make_unique<boost::asio::thread_pool>(io_threads);
    cpu_thread_pool_ = std::make_unique<boost::asio::thread_pool>(cpu_threads);
}

AsyncExecutor::~AsyncExecutor() {
    stop();
}

void AsyncExecutor::stop() {
    if (running_.exchange(false)) {
        // 关闭线程池
        if (io_thread_pool_) {
            io_thread_pool_->stop();
            io_thread_pool_->join();
        }
        
        if (cpu_thread_pool_) {
            cpu_thread_pool_->stop();
            cpu_thread_pool_->join();
        }
    }
}

void AsyncExecutor::wait() {
    if (io_thread_pool_) {
        io_thread_pool_->join();
    }
    
    if (cpu_thread_pool_) {
        cpu_thread_pool_->join();
    }
}

void AsyncExecutor::setThreadPoolSize(PoolType pool_type, size_t size) {
    // 这里简化实现，实际可能需要更复杂的逻辑
    // 因为boost::asio::thread_pool的大小在创建后无法直接修改
    // 完整实现可能需要重建线程池
    (void)pool_type;
    (void)size;
}

size_t AsyncExecutor::getThreadPoolSize(PoolType pool_type) const {
    const auto& pool = getThreadPool(pool_type);
    // boost::asio::thread_pool没有直接获取大小的方法
    // 这里简化实现，返回一个估计值
    return 0;
}

boost::asio::thread_pool& AsyncExecutor::getThreadPool(PoolType pool_type) {
    switch (pool_type) {
        case PoolType::IO:
            return *io_thread_pool_;
        case PoolType::CPU:
            return *cpu_thread_pool_;
        default:
            return *cpu_thread_pool_;
    }
}

const boost::asio::thread_pool& AsyncExecutor::getThreadPool(PoolType pool_type) const {
    switch (pool_type) {
        case PoolType::IO:
            return *io_thread_pool_;
        case PoolType::CPU:
            return *cpu_thread_pool_;
        default:
            return *cpu_thread_pool_;
    }
}

} // namespace protocol
