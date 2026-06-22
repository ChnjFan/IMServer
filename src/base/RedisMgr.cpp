//
// Created by Fan on 2026/5/6.
//

#include "RedisMgr.h"

#include <iostream>
#include <json/value.h>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "ConfigMgr.h"
#include "const.h"

using boost::uuids::uuid;
using boost::uuids::random_generator;

RedisPool::RedisPool(size_t poolSize, const char *host, const int port, const char* password)
    : stop_(false), start_(false), host_(host), passwd_(password), port_(port), capacity_(poolSize) {
    std::cout << "Creating RedisPool ... ";
    try {
        createPool();
        if (connections_.empty()) {
            std::cout << "failed!" << std::endl;
        }
        start_.store(true);
        std::cout << "OK, size = " << connections_.size() << std::endl;
    } catch (...) {
        start_.store(false);
        std::cout << "failed!" << std::endl;
    }
}

RedisPool::~RedisPool() {
    std::lock_guard<std::mutex> guard(mutex_);
    std::cout << "Destroying RedisPool ... ";
    while (!connections_.empty()) {
        connections_.pop();
    }
    std::cout << "OK" << std::endl;
}

redisContext * RedisPool::getConnection() {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait_for(lock, std::chrono::milliseconds(3000), [this] {
        if (stop_.load()) {// 已经停止
            return true;
        }
        return !connections_.empty();
    });
    if (stop_.load()) {
        return nullptr;
    }
    if (start_.load()) {
        createPool();
    }

    if (connections_.empty()) {
        return nullptr;
    }
    const auto conn = connections_.front();
    connections_.pop();
    return conn;
}

void RedisPool::returnConnection(redisContext *c) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (stop_.load()) {
        redisFree(c);// 停止后还回来的连接要释放
        return;
    }
    connections_.push(c);
    cond_.notify_one();// 通知阻塞线程
}

void RedisPool::close() {
    stop_.store(true);
    cond_.notify_all();
}

void RedisPool::createPool() {
    for (int i = 0; i < capacity_; i++) {
        auto conn = redisConnect(host_.c_str(), port_);
        if (conn == nullptr || conn->err) {
            redisFree(conn);
            continue;
        }
        // 认证连接
        const auto reply = static_cast<redisReply *>(redisCommand(conn, "AUTH %s", passwd_.c_str()));
        if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
            std::cout << "RedisPool::redisConnect() AUTH failed" << std::endl;
            redisFree(conn);
            freeReplyObject(reply);
            continue;
        }

        freeReplyObject(reply);
        connections_.push(conn);
    }

    if (!connections_.empty()) {
        start_.store(true);
    }
}

void RedisPool::recreatePool() {
    std::cout << "Recreate RedisPool ... ";
    try {
        createPool();
        start_.store(true);
        std::cout << "OK, size = " << connections_.size() << std::endl;
    } catch (...) {
        start_.store(false);
        std::cout << "failed!" << std::endl;
    }
}

void RedisPool::checkConnection() {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!start_.load()) {
        recreatePool();
        return;
    }

    const size_t poolSize = connections_.size();
    for (int i = 0; i < poolSize; i++) {
        auto conn = connections_.front();
        connections_.pop();

        try {
            const auto reply = static_cast<redisReply *>(redisCommand(conn, "PING"));
            if (conn->err == 0 && reply != nullptr && reply->type != REDIS_REPLY_ERROR) {
                freeReplyObject(reply);
                connections_.push(conn);
                continue;
            }

            freeReplyObject(reply);
            redisFree(conn);
            conn = redisConnect(host_.c_str(), port_);
            if (conn) {
                connections_.push(conn);
            }
        } catch (std::exception &e) {
            std::cout << "RedisPool::checkConnection() PING failed" << std::endl;
            redisFree(conn);
        }
    }
}

RedisMgr::~RedisMgr() {
    close();
}

bool RedisMgr::get(const std::string &key, std::string &value) {
    const auto conn = redisPool_->getConnection();
    if (conn == nullptr) {
        return false;
    }
    Defer defer([&conn, this] {
        redisPool_->returnConnection(conn);
    });
    
    auto reply = static_cast<redisReply *>(redisCommand(conn, "GET %s", key.c_str()));
    if (nullptr == reply || reply->type != REDIS_REPLY_STRING) {
        std::cout << "RedisMgr::get: RedisCommand() GET ["<< key <<"] failed!" << std::endl;
        freeReplyObject(reply);
        reply = nullptr;
        return false;
    }

    value = reply->str;
    freeReplyObject(reply);
    reply = nullptr;

    std::cout << "RedisMgr::get: RedisCommand() GET [" << key << "] OK!" << std::endl;

    return true;
}

bool RedisMgr::set(const std::string &key, const std::string &value) {
    const auto conn = redisPool_->getConnection();
    if (conn == nullptr) {
        return false;
    }
    Defer defer([&conn, this] {
        redisPool_->returnConnection(conn);
    });
    
    auto reply = static_cast<redisReply *>(redisCommand(conn, "SET %s %s", key.c_str(), value.c_str()));
    if (nullptr == reply) {
        std::cout << "RedisMgr::set: RedisCommand() SET ["<< key << " : " << value << "] failed!" << std::endl;
        freeReplyObject(reply);
        reply = nullptr;
        return false;
    }

    if (!(reply->type == REDIS_REPLY_STATUS && (0 == strcmp(reply->str, "OK") || 0 == strcmp(reply->str, "ok")))) {
        std::cout << "RedisMgr::set: RedisCommand() SET ["<< key << " : " << value << "] failed!" << std::endl;
        freeReplyObject(reply);
        reply = nullptr;
        return false;
    }

    freeReplyObject(reply);
    reply = nullptr;
    std::cout << "RedisMgr::set: RedisCommand() SET [" << key << " : " << value << "] OK!" << std::endl;
    return true;
}

bool RedisMgr::lPush(const std::string &key, const std::string &value) {
    const auto conn = redisPool_->getConnection();
    if (conn == nullptr) {
        return false;
    }
    Defer defer([&conn, this] {
        redisPool_->returnConnection(conn);
    });

    auto reply = static_cast<redisReply *>(redisCommand(conn, "LPUSH %s %s", key.c_str(), value.c_str()));
    if (nullptr == reply) {
        std::cout << "RedisMgr::lPush: RedisCommand() LPUSH ["<< key << " : " << value <<"] failed!" << std::endl;
        freeReplyObject(reply);
        reply = nullptr;
        return false;
    }

    if (reply->type != REDIS_REPLY_INTEGER || reply->integer < 0) {
        std::cout << "RedisMgr::lPush: RedisCommand() LPUSH ["<< key << " : " << value << "] failed!" << std::endl;
        freeReplyObject(reply);
        reply = nullptr;
        return false;
    }

    freeReplyObject(reply);
    reply = nullptr;
    std::cout << "RedisMgr::lPush: RedisCommand() LPUSH [" << key << " : " << value << "] OK!" << std::endl;
    return true;
}

bool RedisMgr::lPop(const std::string &key, std::string &value) {
    const auto conn = redisPool_->getConnection();
    if (conn == nullptr) {
        return false;
    }
    Defer defer([&conn, this] {
        redisPool_->returnConnection(conn);
    });

    auto reply = static_cast<redisReply *>(redisCommand(conn, "LPOP %s", key.c_str()));
    if (nullptr == reply || reply->type == REDIS_REPLY_NIL) {
        std::cout << "RedisMgr::lPop: RedisCommand() LPOP ["<< key <<"] failed!" << std::endl;
        freeReplyObject(reply);
        reply = nullptr;
        return false;
    }

    value = reply->str;
    freeReplyObject(reply);
    reply = nullptr;
    std::cout << "RedisMgr::lPop: RedisCommand() LPOP [" << key << " : " << value << "] OK!" << std::endl;
    return true;
}

bool RedisMgr::rPush(const std::string &key, const std::string &value) {
    const auto conn = redisPool_->getConnection();
    if (conn == nullptr) {
        return false;
    }
    Defer defer([&conn, this] {
        redisPool_->returnConnection(conn);
    });

    auto reply = static_cast<redisReply *>(redisCommand(conn, "RPUSH %s %s", key.c_str(), value.c_str()));
    if (nullptr == reply) {
        std::cout << "RedisMgr::rPush: RedisCommand() RPUSH ["<< key << " : " << value <<"] failed!" << std::endl;
        freeReplyObject(reply);
        reply = nullptr;
        return false;
    }

    if (reply->type != REDIS_REPLY_INTEGER || reply->integer < 0) {
        std::cout << "RedisMgr::rPush: RedisCommand() RPUSH ["<< key << " : " << value <<"] failed!" << std::endl;
        freeReplyObject(reply);
        reply = nullptr;
        return false;
    }

    freeReplyObject(reply);
    reply = nullptr;
    std::cout << "RedisMgr::rPush: RedisCommand() RPUSH [" << key << " : " << value << "] OK!" << std::endl;
    return true;
}

bool RedisMgr::rPop(const std::string &key, std::string &value) {
    const auto conn = redisPool_->getConnection();
    if (conn == nullptr) {
        return false;
    }
    Defer defer([&conn, this] {
        redisPool_->returnConnection(conn);
    });

    auto reply = static_cast<redisReply *>(redisCommand(conn, "RPOP %s", key.c_str()));
    if (nullptr == reply || reply->type == REDIS_REPLY_NIL) {
        std::cout << "RedisMgr::rPop: RedisCommand() RPOP ["<< key <<"] failed!" << std::endl;
        freeReplyObject(reply);
        reply = nullptr;
        return false;
    }

    value = reply->str;
    freeReplyObject(reply);
    reply = nullptr;
    std::cout << "RedisMgr::rPop: RedisCommand() RPOP [" << key << " : " << value << "] OK!" << std::endl;
    return true;
}

bool RedisMgr::sAdd(const std::string &key, const std::string &value) {
    const auto conn = redisPool_->getConnection();
    if (conn == nullptr) {
        return false;
    }
    Defer defer([&conn, this] {
        redisPool_->returnConnection(conn);
    });

    const auto reply = static_cast<redisReply *>(redisCommand(conn, "SADD %s %s", key.c_str(), value.c_str()));
    if (nullptr == reply || reply->type != REDIS_REPLY_INTEGER) {
        std::cout << "RedisMgr::hSet: RedisCommand() SADD ["<< key << " : " << value <<"] failed!" << std::endl;
        freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    return true;
}

bool RedisMgr::sRem(const std::string &key, const std::string &value) {
    const auto conn = redisPool_->getConnection();
    if (conn == nullptr) {
        return false;
    }
    Defer defer([&conn, this] {
        redisPool_->returnConnection(conn);
    });

    const auto reply = static_cast<redisReply *>(redisCommand(conn, "SREM %s %s", key.c_str(), value.c_str()));
    if (nullptr == reply || reply->type != REDIS_REPLY_INTEGER) {
        std::cout << "RedisMgr::sRem: RedisCommand() SREM ["<< key << " : " << value <<"] failed!" << std::endl;
        freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    return true;
}

bool RedisMgr::sIsMember(const std::string &key, const std::string &value) {
    const auto conn = redisPool_->getConnection();
    if (conn == nullptr) {
        return false;
    }
    Defer defer([&conn, this] {
        redisPool_->returnConnection(conn);
    });

    const auto reply = static_cast<redisReply *>(redisCommand(conn, "SISMEMBER %s %s", key.c_str(), value.c_str()));
    if (nullptr == reply || reply->type != REDIS_REPLY_INTEGER) {
        std::cout << "RedisMgr::sIsMember: RedisCommand() SISMEMBER ["<< key << " : " << value <<"] failed!" << std::endl;
        freeReplyObject(reply);
        return false;
    }

    const bool result = (reply->integer == 1);
    freeReplyObject(reply);
    return result;
}

bool RedisMgr::hSet(const std::string &key, const std::string &hKey, const std::string &value) {
    const auto conn = redisPool_->getConnection();
    if (conn == nullptr) {
        return false;
    }
    Defer defer([&conn, this] {
        redisPool_->returnConnection(conn);
    });

    auto reply = static_cast<redisReply *>(redisCommand(conn,
        "HSET %s %s %s", key.c_str(), hKey.c_str(), value.c_str()));
    if (nullptr == reply || reply->type != REDIS_REPLY_INTEGER) {
        std::cout << "RedisMgr::hSet: RedisCommand() HSET ["<< key << " : " << value <<"] failed!" << std::endl;
        freeReplyObject(reply);
        reply = nullptr;
        return false;
    }

    freeReplyObject(reply);
    reply = nullptr;
    std::cout << "RedisMgr::hSet: RedisCommand() HSET [" << key << " : " << value << "] OK!" << std::endl;
    return true;
}

bool RedisMgr::hSet(const std::string &key, const std::unordered_map<std::string, std::string> &values) {
    const size_t size = values.size() * 2 + 2;  // 注意内存大小，values 的 key 和 value 都要加入数组中
    auto argv = new const char *[size];
    size_t *argv_size = new size_t[size];
    int index = 0;
    argv[index] = "HSET";
    argv_size[index++] = 4;
    argv[index] = key.c_str();
    argv_size[index++] = key.length();
    for (auto& [hkey, kValue] : values) {
        argv[index] = hkey.c_str();
        argv_size[index++] = hkey.length();
        argv[index] = kValue.c_str();
        argv_size[index++] = kValue.length();
    }

    const auto conn = redisPool_->getConnection();
    if (conn == nullptr) {
        return false;
    }
    Defer defer([&conn, argv, argv_size, this] {
        redisPool_->returnConnection(conn);
        delete[] argv;
        delete[] argv_size;
    });

    const auto reply = static_cast<redisReply *>(redisCommandArgv(conn, size, argv, argv_size));
    if (nullptr == reply || reply->type != REDIS_REPLY_INTEGER) {
        std::cout << "RedisMgr::hSet: RedisCommandArgv() failed! " << std::endl;
        freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    std::cout << "RedisMgr::hSet: RedisCommandArgv() OK!" << std::endl;
    return true;
}

bool RedisMgr::hSet(const char *key, const char *hKey, const char *hValue, size_t hSize) {
    const char* argv[4];
    size_t argv_size[4];
    argv[0] = "HSET";
    argv_size[0] = 4;
    argv[1] = key;
    argv_size[1] = strlen(key);
    argv[2] = hKey;
    argv_size[2] = strlen(hKey);
    argv[3] = hValue;
    argv_size[3] = hSize;

    const auto conn = redisPool_->getConnection();
    if (conn == nullptr) {
        return false;
    }
    Defer defer([&conn, this] {
        redisPool_->returnConnection(conn);
    });

    auto reply = static_cast<redisReply *>(redisCommandArgv(conn, 4, argv, argv_size));
    if (nullptr == reply || reply->type != REDIS_REPLY_INTEGER) {
        std::cout << "RedisMgr::hSet: RedisCommandArgv() failed!" << std::endl;
        freeReplyObject(reply);
        reply = nullptr;
        return false;
    }

    freeReplyObject(reply);
    reply = nullptr;
    std::cout << "RedisMgr::hSet: RedisCommandArgv() OK!" << std::endl;
    return true;
}

bool RedisMgr::hDel(const std::string &key, const std::string &hKey) {
    const auto conn = redisPool_->getConnection();
    if (conn == nullptr) {
        return false;
    }
    Defer defer([&conn, this] {
        redisPool_->returnConnection(conn);
    });

    const auto reply = static_cast<redisReply *>(redisCommand(conn, "HDEL %s %s", key.c_str(), hKey.c_str()));
    if (nullptr == reply) {
        std::cout << "RedisMgr::hDel: RedisCommand() [" << key << ", " << hKey <<"] failed!" << std::endl;
        freeReplyObject(reply);
        return false;
    }
    if (reply->type == REDIS_REPLY_INTEGER && reply->integer <= 0) {
        freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    std::cout << "RedisMgr::del: RedisCommand() [" << key << "] OK!" << std::endl;
    return true;
}

std::string RedisMgr::hGet(const std::string &key, const std::string &hKey) {
    const char* argv[3];
    size_t argv_size[3];
    argv[0] = "HGET";
    argv_size[0] = 4;
    argv[1] = key.c_str();
    argv_size[1] = key.length();
    argv[2] = hKey.c_str();
    argv_size[2] = hKey.length();

    const auto conn = redisPool_->getConnection();
    if (conn == nullptr) {
        return "";
    }
    Defer defer([&conn, this] {
        redisPool_->returnConnection(conn);
    });

    auto reply = static_cast<redisReply *>(redisCommandArgv(conn, 3, argv, argv_size));
    if (nullptr == reply || reply->type == REDIS_REPLY_NIL) {
        std::cout << "RedisMgr::hGet: RedisCommandArgv() failed!" << std::endl;
        freeReplyObject(reply);
        reply = nullptr;
        return "";
    }

    std::string value = reply->str;
    freeReplyObject(reply);
    reply = nullptr;
    std::cout << "RedisMgr::hGet: RedisCommandArgv() [" << key << ", " << hKey << " = " << value << "] OK!" << std::endl;
    return value;
}

std::unordered_map<std::string, std::string> RedisMgr::hGetAll(const std::string &key,
    const std::vector<std::string> &hkeys) {
    const auto conn = redisPool_->getConnection();
    if (conn == nullptr) {
        return {};
    }
    Defer defer([&conn, this] {
        redisPool_->returnConnection(conn);
    });

    const auto reply = static_cast<redisReply *>(redisCommand(conn, "HGETALL %s", key.c_str()));
    if (!reply || reply->type != REDIS_REPLY_ARRAY || reply->elements % 2 != 0) {
        freeReplyObject(reply);
        return {};
    }

    std::unordered_map<std::string, std::string> result;
    for (int i = 0; i < reply->elements; i += 2) {
        std::string field = reply->element[i]->str;
        const std::string value = reply->element[i+1]->str;
        result[field] = value;
    }

    freeReplyObject(reply);
    return result;
}

bool RedisMgr::zSet(const std::string &key, long long score, const std::string &value) {
    const auto conn = redisPool_->getConnection();
    if (conn == nullptr) {
        return false;
    }
    Defer defer([&conn, this] {
        redisPool_->returnConnection(conn);
    });

    auto reply = static_cast<redisReply *>(redisCommand(conn,
        "ZADD %s %lld %s", key.c_str(), score, value.c_str()));
    if (nullptr == reply || reply->type != REDIS_REPLY_INTEGER) {
        std::cout << "RedisMgr::zSet: RedisCommand() ZADD ["<< key << " : " << value <<"] failed!" << std::endl;
        freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    reply = nullptr;
    std::cout << "RedisMgr::zSet: RedisCommand() HSET [" << key << " : " << value << "] OK!" << std::endl;
    return true;
}

bool RedisMgr::zRevrange(const std::string &key, std::vector<std::string> &values, int start, int end) {
    const auto conn = redisPool_->getConnection();
    if (conn == nullptr) {
        return false;
    }
    Defer defer([&conn, this] {
        redisPool_->returnConnection(conn);
    });

    auto reply = static_cast<redisReply *>(redisCommand(conn,
        "ZREVRANGE %s %d %d", key.c_str(), start, end));
    if (nullptr == reply || reply->type != REDIS_REPLY_ARRAY) {
        std::cout << "RedisMgr::zSet: RedisCommand() ZREVRANGE ["<< key << "] failed!" << std::endl;
        freeReplyObject(reply);
        return false;
    }

    for (int i = 0; i < reply->elements; ++i) {
        values.push_back(reply->element[i]->str);
    }

    freeReplyObject(reply);
    reply = nullptr;
    std::cout << "RedisMgr::zSet: RedisCommand() ZREVRANGE [" << key << "] OK!" << std::endl;
    return true;
}

bool RedisMgr::zRem(const std::string &key, const std::string &value) {
    const auto conn = redisPool_->getConnection();
    if (conn == nullptr) {
        return false;
    }
    Defer defer([&conn, this] {
        redisPool_->returnConnection(conn);
    });

    auto reply = static_cast<redisReply *>(redisCommand(conn, "ZREM %s %s", key.c_str(), value.c_str()));
    if (nullptr == reply || reply->type != REDIS_REPLY_INTEGER) {
        std::cout << "RedisMgr::zRem: RedisCommand() [" << key << "] failed!" << std::endl;
        freeReplyObject(reply);
        reply = nullptr;
        return false;
    }

    freeReplyObject(reply);
    reply = nullptr;
    std::cout << "RedisMgr::zRem: RedisCommand() [" << key << "] OK!" << std::endl;
    return true;
}

bool RedisMgr::del(const std::string &key) const {
    const auto conn = redisPool_->getConnection();
    if (conn == nullptr) {
        return false;
    }
    Defer defer([&conn, this] {
        redisPool_->returnConnection(conn);
    });

    auto reply = static_cast<redisReply *>(redisCommand(conn, "DEL %s", key.c_str()));
    if (nullptr == reply || reply->type != REDIS_REPLY_INTEGER) {
        std::cout << "RedisMgr::del: RedisCommand() [" << key << "] failed!" << std::endl;
        freeReplyObject(reply);
        reply = nullptr;
        return false;
    }

    freeReplyObject(reply);
    reply = nullptr;
    std::cout << "RedisMgr::del: RedisCommand() [" << key << "] OK!" << std::endl;
    return true;
}

bool RedisMgr::existsKey(const std::string &key) const {
    const auto conn = redisPool_->getConnection();
    if (conn == nullptr) {
        return false;
    }
    Defer defer([&conn, this] {
        redisPool_->returnConnection(conn);
    });

    const auto reply = static_cast<redisReply *>(redisCommand(conn, "exists %s", key.c_str()));
    if (nullptr == reply || reply->type != REDIS_REPLY_INTEGER || reply->integer == 0) {
        std::cout << "RedisMgr::existsKey: RedisCommand() [" << key << "] not found!" << std::endl;
        freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    std::cout << "RedisMgr::existsKey: RedisCommand() [" << key << "] OK!" << std::endl;
    return true;
}

bool RedisMgr::setExpire(const std::string &key, const int expire) const {
    const auto conn = redisPool_->getConnection();
    if (conn == nullptr) {
        return false;
    }
    Defer defer([&conn, this] {
        redisPool_->returnConnection(conn);
    });

    const auto reply = static_cast<redisReply *>(redisCommand(conn, "EXPIRE %s %d", key.c_str(), expire));
    if (nullptr == reply || reply->type != REDIS_REPLY_INTEGER || reply->integer == 0) {
        std::cout << "RedisMgr::setExpire: RedisCommand() [" << key << "] not found!" << std::endl;
        freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    std::cout << "RedisMgr::setExpire: RedisCommand() [" << key << "expire: " << expire <<"] OK!" << std::endl;
    return true;
}

bool RedisMgr::clearExpire(const std::string &key) const {
    const auto conn = redisPool_->getConnection();
    if (conn == nullptr) {
        return false;
    }
    Defer defer([&conn, this] {
        redisPool_->returnConnection(conn);
    });

    const auto reply = static_cast<redisReply *>(redisCommand(conn, "PERSIST %s %d", key.c_str()));
    if (nullptr == reply || reply->type != REDIS_REPLY_INTEGER || reply->integer == 0) {
        std::cout << "RedisMgr::clearExpire: RedisCommand() [" << key << "] not found!" << std::endl;
        freeReplyObject(reply);
        return false;
    }

    freeReplyObject(reply);
    std::cout << "RedisMgr::clearExpire: RedisCommand() [" << key <<"] OK!" << std::endl;
    return true;
}

std::string RedisMgr::acquireLock(const std::string& name, const int timeout, const int acquireTimeout) const {
    const auto conn = redisPool_->getConnection();
    if (conn == nullptr) {
        return "";
    }
    Defer defer([&conn, this] {
        redisPool_->returnConnection(conn);
    });

    random_generator generator;
    std::string identifier = boost::uuids::to_string(generator());
    const auto endTime = std::chrono::steady_clock::now() + std::chrono::seconds(acquireTimeout);

    while (std::chrono::steady_clock::now() < endTime) {
        const auto reply = static_cast<redisReply *>(redisCommand(conn, "SET %s %s NX EX %d",
            name.c_str(), identifier.c_str(), timeout));
        if (reply != nullptr) {
            if (reply->type == REDIS_REPLY_STATUS && std::string(reply->str) == "OK") {
                freeReplyObject(reply);
                return identifier;
            }
            freeReplyObject(reply);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    std::cout << "AcquireLock: RedisCommand() [" << name << "] error" << std::endl;
    return "";
}

bool RedisMgr::releaseLock(const std::string &name, const std::string& identifier) const {
    const auto conn = redisPool_->getConnection();
    if (conn == nullptr) {
        return false;
    }
    Defer defer([&conn, this] {
        redisPool_->returnConnection(conn);
    });
    // 通过 lua 判断锁标识是否匹配，匹配则删除锁
    const auto luaScript = R"(if redis.call('get', KEYS[1]) == ARGV[1] then
                                return redis.call('del', KEYS[1])
                            else
                                return 0
                            end)";
    const auto reply = static_cast<redisReply *>(redisCommand(conn, "EVAL %s 1 %s %s",
        luaScript, name.c_str(), identifier.c_str()));
    if (reply) {
        if (reply->type == REDIS_REPLY_INTEGER && reply->integer == 1) {
            freeReplyObject(reply);
            return true;
        }
        else if (reply->str) {
            std::cout << "ReleaseLock: RedisCommand() [" << name << ": " << identifier << "] error: " << reply->str << std::endl;
        }
        freeReplyObject(reply);
    }
    return false;
}

void RedisMgr::close() const {
    redisPool_->close();
}

RedisMgr::RedisMgr() {
    auto& config = ConfigMgr::getInstance();
    size_t poolSize = DEFAULT_REDIS_POOL_SIZE;
    if (!config["Redis"]["PoolSize"].empty()) {
        poolSize = std::stoi(config["Redis"]["PoolSize"]);
    }

    const std::string host = config["Redis"]["Host"];
    int port = std::stoi(config["Redis"]["Port"]);
    const std::string password = config["Redis"]["Password"];
    redisPool_ = std::make_unique<RedisPool>(poolSize, host.c_str(), port, password.c_str());
}
