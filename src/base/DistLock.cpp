//
// Created by Fan on 2026/6/11.
//

#include "DistLock.h"

#include <iostream>
#include <utility>

#include "RedisMgr.h"

DistLock::DistLock(std::string name, const int timeout, const int acquireTimeout)
    : name_(std::move(name)), timeout_(timeout), acquireTimeout_(acquireTimeout) {}

bool DistLock::lock() {
    identifier_ = RedisMgr::getInstance()->acquireLock(name_, timeout_, acquireTimeout_);
    return !identifier_.empty();
}

bool DistLock::unlock() const {
    return RedisMgr::getInstance()->releaseLock(name_, identifier_);
}

DistLockGuard::DistLockGuard(const std::string &name, const int timeout, const int acquireTimeout)
    : name_(name), lock_(name, timeout, acquireTimeout) {
    if (!lock_.lock()) {
        throw std::logic_error("Distribute lock" + name +" failed");
    }
}

DistLockGuard::~DistLockGuard() {
    if (!lock_.unlock()) {
        std::cerr << "Distribute unlock " + name_ +" failed" << std::endl;
    }
}




