/**
 * @file ConnectionLimiter.cpp
 * @brief 连接层流量控制
 * @author ChnjFan
 * @date 2024-12-04
 * @copyright Copyright (c) 2024 ChnjFan. All rights reserved.
 */

#include "ConnectionLimiter.h"

ConnectionLimiter* ConnectionLimiter::instance = nullptr;
Poco::Mutex ConnectionLimiter::mutex;

ConnectionLimiter* ConnectionLimiter::getInstance() {
    if (instance == nullptr) {
        Poco::Mutex::ScopedLock lock(mutex);
        if (instance == nullptr) {
            instance = new ConnectionLimiter();
        }
    }
    return instance;
}

void ConnectionLimiter::destroyInstance() {
    Poco::Mutex::ScopedLock lock(mutex);
    delete instance;
    instance = nullptr;
}

ConnectionLimiter::ConnectionLimiter() = default;

bool ConnectionLimiter::isIPAllowed(const std::string& ip) {
    Poco::Mutex::ScopedLock lock(mutex);
    auto it = ipRecords.find(ip);
    
    if (it == ipRecords.end()) {
        return true;
    }

    // 检查是否被封禁
    if (isIPBanned(ip)) {
        return false;
    }

    // 检查是否需要重置计数
    Poco::Timestamp now;
    if ((now - it->second.lastReset) > RESET_INTERVAL) {
        resetIPCounter(ip);
        return true;
    }

    return it->second.connectionCount < MAX_CONNECTIONS_PER_IP;
}

void ConnectionLimiter::recordConnection(const std::string& ip) {
    Poco::Mutex::ScopedLock lock(mutex);
    auto& record = ipRecords[ip];
    record.connectionCount++;
    if (record.lastReset.isElapsed(0)) {
        record.lastReset.update();
    }
}

void ConnectionLimiter::recordAuthFailure(const std::string& ip) {
    Poco::Mutex::ScopedLock lock(mutex);
    auto& record = ipRecords[ip];
    record.authFailureCount++;
    
    if (record.authFailureCount >= MAX_AUTH_FAILURES) {
        // 设置封禁时间
        record.banUntil.update();
        record.banUntil += Poco::Timespan(0, 0, 0, BAN_DURATION, 0);
    }
}

bool ConnectionLimiter::isIPBanned(const std::string& ip) {
    auto it = ipRecords.find(ip);
    if (it == ipRecords.end()) {
        return false;
    }

    Poco::Timestamp now;
    return now < it->second.banUntil;
}

void ConnectionLimiter::resetIPCounter(const std::string& ip) {
    auto& record = ipRecords[ip];
    record.connectionCount = 0;
    record.lastReset.update();
}
