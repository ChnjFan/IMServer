//
// Created by fan on 24-12-30.
//

#include "ServerConnectionLimiter.h"

ServerNet::ServerConnectionLimiter* ServerNet::ServerConnectionLimiter::instance = nullptr;
Poco::Mutex ServerNet::ServerConnectionLimiter::mutex;

ServerNet::ServerConnectionLimiter* ServerNet::ServerConnectionLimiter::getInstance() {
    if (instance == nullptr) {
        Poco::Mutex::ScopedLock lock(mutex);
        if (instance == nullptr) {
            instance = new ServerConnectionLimiter();
        }
    }
    return instance;
}

void ServerNet::ServerConnectionLimiter::destroyInstance() {
    Poco::Mutex::ScopedLock lock(mutex);
    delete instance;
    instance = nullptr;
}

void ServerNet::ServerConnectionLimiter::setLimiter(bool flag) {
    isOpen = flag;
}

bool ServerNet::ServerConnectionLimiter::isIPAllowed(const std::string& ip) {
    Poco::Mutex::ScopedLock lock(mutex);
    if (!isOpen)
        return true;

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

void ServerNet::ServerConnectionLimiter::recordConnection(const std::string& ip) {
    Poco::Mutex::ScopedLock lock(mutex);
    if (!isOpen)
        return;
    auto& record = ipRecords[ip];
    record.connectionCount++;
    if (record.lastReset.isElapsed(0)) {
        record.lastReset.update();
    }
}

void ServerNet::ServerConnectionLimiter::recordAuthFailure(const std::string& ip) {
    Poco::Mutex::ScopedLock lock(mutex);
    if (!isOpen)
        return;
    auto& record = ipRecords[ip];
    record.authFailureCount++;

    if (record.authFailureCount >= MAX_AUTH_FAILURES) {
        // 设置封禁时间
        record.banUntil.update();
        record.banUntil += Poco::Timespan(0, 0, 0, BAN_DURATION, 0);
    }
}

bool ServerNet::ServerConnectionLimiter::isIPBanned(const std::string& ip) {
    if (!isOpen)
        return false;
    auto it = ipRecords.find(ip);
    if (it == ipRecords.end()) {
        return false;
    }

    Poco::Timestamp now;
    return now < it->second.banUntil;
}

void ServerNet::ServerConnectionLimiter::resetIPCounter(const std::string& ip) {
    if (!isOpen)
        return;
    auto& record = ipRecords[ip];
    record.connectionCount = 0;
    record.lastReset.update();
}
