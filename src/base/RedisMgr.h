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

// 用户在线状态
#define USER_ONLINE_INFO_PREFIX "user_online_"
#define USER_ONLINE_SERVER_NAME "server_name"
#define USER_ONLINE_TOKEN "token"
#define USER_SESSION_ID "session_id"
// 用户信息
#define UID_INDEX_MAP_PREFIX "uid_index_"
#define USER_BASE_INFO_PREFIX "user_base_info_"
#define USER_PROFILE_INFO_PREFIX "user_profile_info_"
#define LOGIN_COUNT "login_chat_server_count"

// 聊天会话缓存
#define CHAT_CONVER_PREFIX "chat_conver_"
#define CHAT_CONVER_INFO_PREFIX "conver_info_"

// 分布式锁
#define DIST_LOCK_PREFIX "lock_"
#define DIST_LOCK_SERVER_COUNT "lock_server_count"
#define DIST_LOCK_TIMEOUT 10
#define DIST_ACQUIRE_TIMEOUT 5

class RedisPool {
public:
    RedisPool(size_t poolSize, const char* host, int port, const char* password);
    ~RedisPool();
    redisContext* getConnection();
    void returnConnection(redisContext* c);
    void close();

private:
    void createPool();
    void recreatePool();
    void checkConnection();

    std::atomic<bool> stop_;
    std::atomic<bool> start_;   // 启动标记，如果没有成功创建 Redis 连接，之后请求重新尝试连接
    std::string host_;
    std::string passwd_;
    int port_;
    int capacity_;
    std::queue<redisContext*> connections_;
    std::mutex mutex_;
    std::condition_variable cond_;
    std::thread thread_;
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

    bool del(const std::string& key) const;
    bool existsKey(const std::string& key) const;

    std::string acquireLock(const std::string& name, int timeout, int acquireTimeout) const;
    bool releaseLock(const std::string& name, const std::string& identifier) const;

    void close() const;


private:
    friend class Singleton<RedisMgr>;
    RedisMgr();

    std::unique_ptr<RedisPool> redisPool_;
};


#endif //IMSERVER_REDISMGR_H