#include "event_loop.h"
#include <iostream>
#include <stdexcept>

namespace im {
namespace network {

EventLoop::EventLoop(size_t thread_pool_size)
    : io_context_(),
      work_guard_(boost::asio::make_work_guard(io_context_)),
      thread_pool_size_(thread_pool_size),
      running_(false) {
    // 确保至少有一个线程
    if (thread_pool_size_ < 1) {
        thread_pool_size_ = 1;
    }
}

EventLoop::~EventLoop() {
    stop();
}

void EventLoop::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) {
        return; // 已经在运行，直接返回
    }

    running_ = true;
    threads_.reserve(thread_pool_size_);

    // 创建线程池
    for (size_t i = 0; i < thread_pool_size_; ++i) {
        threads_.emplace_back([this]() {
            try {
                this->threadFunc();
            } catch (const std::exception& e) {
                std::cerr << "EventLoop thread exception: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "EventLoop thread unknown exception" << std::endl;
            }
        });
    }

    std::cout << "EventLoop started with " << thread_pool_size_ << " threads" << std::endl;
}

void EventLoop::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) {
        return;
    }

    running_ = false;
    work_guard_.reset(); // 释放工作守卫，允许io_context退出
    io_context_.stop();  // 停止io_context

    // 等待所有线程结束
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    threads_.clear();

    std::cout << "EventLoop stopped" << std::endl;
}

boost::asio::io_context& EventLoop::getIOContext() {
    return io_context_;
}

size_t EventLoop::getThreadPoolSize() const {
    return thread_pool_size_;
}

bool EventLoop::isRunning() const {
    return running_;
}

void EventLoop::threadFunc() {
    try {
        io_context_.run();
    } catch (const std::exception& e) {
        std::cerr << "EventLoop io_context.run() exception: " << e.what() << std::endl;
        throw;
    }
}

} // namespace network
} // namespace im
