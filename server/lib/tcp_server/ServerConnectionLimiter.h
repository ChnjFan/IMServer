/**
 * @file ConnectionLimiter.cpp
 * @brief 连接层流量控制
 * @author ChnjFan
 * @date 2024-12-04
 * @copyright Copyright (c) 2024 ChnjFan. All rights reserved.
 */

#ifndef IMSERVER_SERVERCONNECTIONLIMITER_H
#define IMSERVER_SERVERCONNECTIONLIMITER_H

#include <string>
#include <unordered_map>
#include "Poco/Timespan.h"
#include "Poco/Mutex.h"

namespace TcpServerNet {

class ServerConnectionLimiter {
public:
    static ServerConnectionLimiter *getInstance();
    static void destroyInstance();

    // 流量控制开关
    void setLimiter(bool flag);
    // 检查IP是否被限流
    bool isIPAllowed(const std::string &ip);
    // 记录IP的连接尝试
    void recordConnection(const std::string &ip);
    // 记录IP的认证失败
    void recordAuthFailure(const std::string &ip);
    // 检查IP是否被封禁
    bool isIPBanned(const std::string &ip);
    // 重置IP的连接计数
    void resetIPCounter(const std::string &ip);

private:
    ServerConnectionLimiter() = default;
    ~ServerConnectionLimiter() = default;

    struct IPRecord {
        int connectionCount;        // 连接次数
        int authFailureCount;      // 认证失败次数
        Poco::Timestamp lastReset; // 最后重置时间
        Poco::Timestamp banUntil;  // 封禁解除时间
    };

    static ServerConnectionLimiter* instance;
    static Poco::Mutex mutex;

    bool isOpen;
    std::unordered_map<std::string, IPRecord> ipRecords;

    // 限流参数
    static constexpr int MAX_CONNECTIONS_PER_IP = 100;     // 每个IP的最大连接数
    static constexpr int MAX_AUTH_FAILURES = 5;           // 最大认证失败次数
    static constexpr int RESET_INTERVAL = 60 * 1000;      // 重置间隔（毫秒）
    static constexpr int BAN_DURATION = 300 * 1000;       // 封禁时长（毫秒）
};

}
#endif //IMSERVER_SERVERCONNECTIONLIMITER_H
