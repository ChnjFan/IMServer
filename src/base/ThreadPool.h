//
// Created by Fan on 2026/6/10.
//

#ifndef IMSERVER_THREADPOOL_H
#define IMSERVER_THREADPOOL_H

#include <memory>
#include <queue>
#include <thread>

class Task {
public:
    virtual ~Task() = default;
    virtual void exec() = 0;
};

class ThreadPool {
public:
    ThreadPool();
    ~ThreadPool();

    void start(int poolSize = 4);
    void stop();
    void addTask(const std::shared_ptr<Task> &task);
private:
    void run();

    bool running_;
    std::queue<std::shared_ptr<Task>> tasks_;
    std::vector<std::shared_ptr<std::thread>> threads_;
    std::mutex mtx_;
    std::condition_variable cond_;
};


#endif //IMSERVER_THREADPOOL_H