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

// 用户缓存
#define USER_IP_PREFIX "user_ip_"
#define USER_TOKEN_PREFIX "user_token_"
#define IP_COUNT_PREFIX "ip_count_"
#define USER_BASE_INFO_PREFIX "user_base_info_"
#define LOGIN_COUNT "login_chat_server_count"

// 聊天会话缓存
#define CHAT_CONVER_PREFIX "chat_conver_"
#define CHAT_CONVER_INFO_PREFIX "conver_info_"

class RedisPool {
public:
    RedisPool(size_t poolSize, const char* host, int port, const char* password);
    ~RedisPool();
    redisContext* getConnection();
    void returnConnection(redisContext* c);
    void close();

private:
    void createPool();

    std::atomic<bool> stop_;
    std::atomic<bool> start_;   // 启动标记，如果没有成功创建 Redis 连接，之后请求重新尝试连接
    std::string host_;
    std::string passwd_;
    int port_;
    int capacity_;
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
    bool hSet(const std::string& key, const std::unordered_map<std::string, std::string>& values);
    // 处理二进制数据
    bool hSet(const char* key, const char* hKey, const char* hValue, size_t hSize);
    bool hDel(const std::string& key, const std::string & hKey);
    std::string hGet(const std::string& key, const std::string& hKey);
    std::unordered_map<std::string, std::string> hGetAll(const std::string& key, const std::vector<std::string>& hkeys);
    // 有序集合
    bool zSet(const std::string& key, long long score, const std::string& value);
    bool zRevrange(const std::string& key, std::vector<std::string>& values, int start, int end);
    bool zRem(const std::string& key, const std::string& value);
    bool del(const std::string& key);
    bool existsKey(const std::string& key);
    void close();


private:
    friend class Singleton<RedisMgr>;
    RedisMgr();

    std::unique_ptr<RedisPool> redisPool_;
};


#endif //IMSERVER_REDISMGR_H