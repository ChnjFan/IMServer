//
// Created by Fan on 2026/5/6.
//

#ifndef IMSERVER_REDISMGR_H
#define IMSERVER_REDISMGR_H

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <hiredis/hiredis.h>

#include "const.h"
#include "Singleton.h"

class RedisPool {
public:
    RedisPool(size_t poolSize, const char* host, int port, const char* password);
    ~RedisPool();
    redisContext* getConnection();
    void returnConnection(redisContext* c);
    void close();

private:
    std::atomic<bool> stop_;
    const char* host_;
    int port_;
    std::queue<redisContext*> connections_;
    std::mutex mutex_;
    std::condition_variable cond_;
};

class RedisMgr : public Singleton<RedisMgr> {
public:
    ~RedisMgr();
    bool get(const std::string& key, std::string& value);
    bool set(const std::string& key, const std::string& value);
    // 队列的入队和出队操作
    bool lPush(const std::string& key, const std::string& value);
    bool lPop(const std::string& key, std::string& value);
    bool rPush(const std::string& key, const std::string& value);
    bool rPop(const std::string& key, std::string& value);
    // 集合
    bool hSet(const std::string& key, const std::string& hKey, const std::string& value);
    // 处理二进制数据
    bool hSet(const char* key, const char* hKey, const char* hValue, size_t hSize);
    bool hDel(const std::string& key, const std::string & hKey);

    std::string hGet(const std::string& key, const std::string& hKey);
    bool del(const std::string& key);
    bool existsKey(const std::string& key);
    void close();


private:
    friend class Singleton<RedisMgr>;
    RedisMgr();

    std::unique_ptr<RedisPool> redisPool_;
};


#endif //IMSERVER_REDISMGR_H