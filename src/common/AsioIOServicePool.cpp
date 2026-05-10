//
// Created by Fan on 2026/5/5.
//

#include "AsioIOServicePool.h"

#include <iostream>

#include "ConfigMgr.h"

AsioIOServicePool::~AsioIOServicePool() {
    std::cout << "Destroying AsioIOServicePool ...";
    stop();
    std::cout << "OK" << std::endl;
}

AsioIOServicePool::IOService & AsioIOServicePool::getIOService() {
    // 使用轮询策略
    auto& service = ioServices_[nextIndex_++];
    if (nextIndex_ == ioServices_.size()) {
        nextIndex_ = 0;
    }
    return service;
}

void AsioIOServicePool::stop() {
    // 先将服务停止，已经绑定读写事件后需要手动 stop 服务
    for (auto& service : ioServices_) {
        service.stop();
    }
    for (auto& work : works_) {
        work.reset();
    }
    // 等待线程结束
    for (auto& thread : threads_) {
        thread.join();
    }
}

AsioIOServicePool::AsioIOServicePool(std::size_t size)
    : ioServices_(size), works_(size), nextIndex_(0){
    std::cout << "Creating AsioIOServicePool, size = " << size << std::endl;
    for (std::size_t i = 0; i < size; i++) {
        works_[i] = std::make_unique<Work>(ioServices_[i].get_executor());
    }

    // 创建线程
    for (std::size_t i = 0; i < size; i++) {
        threads_.emplace_back([this, i]() {
            ioServices_[i].run();
        });
    }
}
