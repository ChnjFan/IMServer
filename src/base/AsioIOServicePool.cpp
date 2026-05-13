#include "AsioIOServicePool.h"

#include <iostream>
#include <thread>

#include "ConfigMgr.h"

AsioIOServicePool::~AsioIOServicePool() {
    std::cout << "Destroying AsioIOServicePool ...";
    stop();
    std::cout << "OK" << std::endl;
}

AsioIOServicePool::IOService & AsioIOServicePool::getIOService() {
    auto& service = ioServices_[nextIndex_++];
    if (nextIndex_ == ioServices_.size()) {
        nextIndex_ = 0;
    }
    return service;
}

void AsioIOServicePool::stop() {
    if (bool expected = false; !stopped_.compare_exchange_strong(expected, true)) {
        return;
    }

    for (auto& service : ioServices_) {
        service.stop();
    }
    for (auto& work : works_) {
        work.reset();
    }
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

AsioIOServicePool::AsioIOServicePool(std::size_t size)
    : ioServices_(size), works_(size), nextIndex_(0){
    std::cout << "Creating AsioIOServicePool, size = " << size << std::endl;
    for (std::size_t i = 0; i < size; i++) {
        works_[i] = std::make_unique<Work>(ioServices_[i].get_executor());
    }

    for (std::size_t i = 0; i < size; i++) {
        threads_.emplace_back([this, i]() {
            ioServices_[i].run();
        });
    }
}
