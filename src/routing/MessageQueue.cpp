#include "MessageQueue.h"
#include <iostream>
#include <chrono>

namespace routing {

MessageQueue::MessageQueue() 
    : max_size_(10000),
      current_size_(0),
      stop_(false),
      thread_count_(0) {
}

MessageQueue::~MessageQueue() {
    stop();
}

void MessageQueue::start(int thread_count) {
    if (stop_) {
        stop_ = false;
    }

    thread_count_ = thread_count;
    
    // 启动工作线程
    for (int i = 0; i < thread_count_; i++) {
        workers_.emplace_back([this]() {
            workerFunction();
        });
    }
}

void MessageQueue::stop() {
    stop_ = true;
    cv_.notify_all();
    
    // 等待所有工作线程结束
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    workers_.clear();
    thread_count_ = 0;
}

bool MessageQueue::enqueue(const QueueItem& item) {
    if (current_size_ >= max_size_) {
        return false;
    }

    std::unique_lock<std::mutex> lock(queue_mutex_);
    
    queue_.push(item);
    current_size_++;
    
    // 通知等待的线程
    cv_.notify_one();
    
    return true;
}

bool MessageQueue::enqueue(const im::common::protocol::RouteRequest& request, 
                         std::function<void(const im::common::protocol::RouteResponse&)> callback, 
                         int priority) {
    QueueItem item(request, callback, priority);
    return enqueue(item);
}

bool MessageQueue::dequeue(QueueItem& item) {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    
    // 等待队列非空或停止
    cv_.wait(lock, [this]() {
        return !queue_.empty() || stop_;
    });
    
    if (stop_ && queue_.empty()) {
        return false;
    }
    
    if (queue_.empty()) {
        return false;
    }
    
    item = queue_.top();
    queue_.pop();
    current_size_--;
    
    return true;
}

void MessageQueue::clear() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    
    while (!queue_.empty()) {
        queue_.pop();
    }
    
    current_size_ = 0;
}

void MessageQueue::workerFunction() {
    while (!stop_) {
        QueueItem item;
        if (dequeue(item)) {
            // 消息队列现在只负责存储消息
            // 处理逻辑由MessageRouter负责
            std::cout << "Message dequeued, waiting for processing..." << std::endl;
        }
    }
}

// 暂时不使用处理器功能
void MessageQueue::setProcessor(MessageProcessor processor) {
    // 预留接口，暂不实现
}

} // namespace routing
