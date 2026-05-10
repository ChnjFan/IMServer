//
// Created by Fan on 2026/5/6.
//

#include "RedisMgr.h"

#include <iostream>
#include <json/value.h>

#include "ConfigMgr.h"

RedisPool::RedisPool(size_t poolSize, const char *host, const int port, const char* password)
    : stop_(false), host_(host), port_(port) {
    std::cout << "Creating RedisPool ... ";
    for (int i = 0; i < poolSize; i++) {
        auto conn = redisConnect(host, port);
        if (conn == nullptr || conn->err) {
            redisFree(conn);
            continue;
        }
        // 认证连接
        const auto reply = static_cast<redisReply *>(redisCommand(conn, "AUTH %s", password));
        if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
            std::cout << "RedisPool::redisConnect() AUTH failed" << std::endl;
            redisFree(conn);
            freeReplyObject(reply);
            continue;
        }

        freeReplyObject(reply);
        connections_.push(conn);
    }
    std::cout << "OK, size = " << connections_.size() << std::endl;
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
    cond_.wait(lock, [this] {
        if (stop_.load()) {// 已经停止
            return true;
        }
        return !connections_.empty();
    });
    if (stop_.load()) {
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

RedisMgr::~RedisMgr() {
    close();
}

bool RedisMgr::get(const std::string &key, std::string &value) {
    const auto conn = redisPool_->getConnection();
    if (conn == nullptr) {
        return false;
    }
    
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

bool RedisMgr::hSet(const std::string &key, const std::string &hKey, const std::string &value) {
    const auto conn = redisPool_->getConnection();
    if (conn == nullptr) {
        return false;
    }

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
    std::cout << "RedisMgr::hGet: RedisCommandArgv() OK!" << std::endl;
    return value;
}

bool RedisMgr::del(const std::string &key) {
    const auto conn = redisPool_->getConnection();
    if (conn == nullptr) {
        return false;
    }

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

bool RedisMgr::existsKey(const std::string &key) {
    const auto conn = redisPool_->getConnection();
    if (conn == nullptr) {
        return false;
    }

    auto reply = static_cast<redisReply *>(redisCommand(conn, "exists %s", key.c_str()));
    if (nullptr == reply || reply->type != REDIS_REPLY_INTEGER || reply->integer == 0) {
        std::cout << "RedisMgr::existsKey: RedisCommand() [" << key << "] not found!" << std::endl;
        freeReplyObject(reply);
        reply = nullptr;
        return false;
    }

    freeReplyObject(reply);
    reply = nullptr;
    std::cout << "RedisMgr::existsKey: RedisCommand() [" << key << "] OK!" << std::endl;
    return true;
}

void RedisMgr::close() {
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
