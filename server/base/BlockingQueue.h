//
// Created by fan on 24-12-8.
//

#ifndef IMSERVER_BLOCKINGQUEUE_H
#define IMSERVER_BLOCKINGQUEUE_H

#include <queue>
#include "Poco/Mutex.h"
#include "Poco/Condition.h"
#include "Poco/ScopedLock.h"

namespace Base {

template<typename T>
class BlockingQueue {
public:
    BlockingQueue() = default;
    ~BlockingQueue() = default;

    // 禁用拷贝构造和赋值操作
    BlockingQueue(const BlockingQueue&) = delete;
    BlockingQueue& operator=(const BlockingQueue&) = delete;

    // 入队操作
    void push(const T& item) {
        Poco::ScopedLock<Poco::Mutex> lock(mutex_);
        queue_.push(item);
        condition_.signal();  // 通知等待的消费者
    }

    // 移动版本的入队操作
    void push(T&& item) {
        Poco::ScopedLock<Poco::Mutex> lock(mutex_);
        queue_.push(std::move(item));
        condition_.signal();  // 通知等待的消费者
    }

    // 出队操作（阻塞）
    T pop() {
        Poco::ScopedLock<Poco::Mutex> lock(mutex_);
        while (queue_.empty()) {
            condition_.wait(mutex_);
        }
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    // 尝试出队操作（非阻塞）
    bool tryPop(T& item) {
        Poco::ScopedLock<Poco::Mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    // 尝试出队操作（带超时）
    bool tryPopFor(T& item, long milliseconds) {
        Poco::ScopedLock<Poco::Mutex> lock(mutex_);
        if (queue_.empty()) {
            if (!condition_.tryWait(mutex_, milliseconds)) {
                return false;  // 超时
            }
            if (queue_.empty()) {
                return false;  // 被虚假唤醒
            }
        }
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    // 检查队列是否为空
    bool empty() const {
        Poco::ScopedLock<Poco::Mutex> lock(mutex_);
        return queue_.empty();
    }

    // 获取队列大小
    size_t size() const {
        Poco::ScopedLock<Poco::Mutex> lock(mutex_);
        return queue_.size();
    }

    // 清空队列
    void clear() {
        Poco::ScopedLock<Poco::Mutex> lock(mutex_);
        std::queue<T> empty;
        std::swap(queue_, empty);
    }

private:
    std::queue<T> queue_;
    mutable Poco::Mutex mutex_;
    Poco::Condition condition_;
};

}


#endif //IMSERVER_BLOCKINGQUEUE_H
