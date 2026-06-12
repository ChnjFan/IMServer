//
// Created by Fan on 2026/6/10.
//

#include "ThreadPool.h"

ThreadPool::ThreadPool() : running_(false) {
}

ThreadPool::~ThreadPool() {
}

void ThreadPool::start(int poolSize) {
    running_ = true;
    if (poolSize <= 0) {
        poolSize = 4;
    }

    for (int i = 0; i < poolSize; ++i) {
        auto thread = std::make_shared<std::thread>([this]() {
            run();
        });
        threads_.push_back(thread);
    }
}

void ThreadPool::stop() {
    running_ = false;
    cond_.notify_all();
    for (const auto& thread : threads_) {
        if (thread->joinable())
            thread->join();
    }
}

void ThreadPool::addTask(const std::shared_ptr<Task> &task) {
    std::lock_guard<std::mutex> lock(mtx_);
    tasks_.push(task);
    cond_.notify_one();
}

void ThreadPool::run() {
    std::shared_ptr<Task> task;
    while (running_) {
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cond_.wait(lock, [this]() {
                if (!running_)
                    return true;
                return !tasks_.empty();
            });

            if (!running_) {
                break;
            }
            task = tasks_.front();
            tasks_.pop();
        }
        if (task) {
            task->exec();
            task.reset();
        }
    }
}
