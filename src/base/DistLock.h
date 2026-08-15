//
// Created by Fan on 2026/6/11.
//

#ifndef IMSERVER_DISTLOCK_H
#define IMSERVER_DISTLOCK_H

#include <string>

class DistLock {
public:
    DistLock(std::string  name, int timeout, int acquireTimeout);
    bool lock();
    bool unlock() const;
private:
    std::string name_;
    int timeout_;
    int acquireTimeout_;
    std::string identifier_;
};

// class DistLockGuard {
// public:
//     DistLockGuard(const std::string& name, int timeout, int acquireTimeout);
//     ~DistLockGuard();
// private:
//     std::string name_;
//     DistLock lock_;
// };


#endif //IMSERVER_DISTLOCK_H