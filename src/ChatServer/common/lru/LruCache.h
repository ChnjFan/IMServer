//
// Created by Fan on 2026/07/24.
//

#ifndef IMSERVER_LRU_CACHE_H
#define IMSERVER_LRU_CACHE_H

#include <string>
#include <mutex>
#include <list>
#include <unordered_map>
#include <functional>
#include <optional>
#include <memory>
#include <thread>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <chrono>

#include "Stats.h"

//==============================================================================
// LruCache<V> — 通用分片 LRU 本地缓存
//
// 特性:
//   - 分片存储 (key hash 取模, 每分片独立 mutex)
//   - TTL 懒过期 (默认 300s, 命中时检查)
//   - LRU 淘汰 + 同步回调
//   - Singleflight (key 级 mutex + condvar, 一线程拉取其他等待)
//   - 逻辑过期 + 异步刷新 (每实例一个刷新线程)
//   - 失败分级: 数据不存在或连续 5 次失败则删除, 否则保留
//   - 监控指标 (独立 Stats 类, atomic counter)
//   - shared_from_this 保证析构安全
//
// 使用方式:
//   auto cache = LruCache<UserInfo>::create(
//       {.maxItems = 5000, .numShards = 16, .ttl = 300s, .blockOnExpire = false},
//       [](const string& key) -> optional<UserInfo> { return loadFromRedis(key); },
//       [](const string& key, UserInfo& val) { redis.set(key, val); }
//   );
//   auto val = cache->get("user:123");
//==============================================================================

template<typename V>
class LruCache : public std::enable_shared_from_this<LruCache<V>> {
public:
    using LoadFunc   = std::function<std::optional<V>(const std::string&)>;
    using EvictFunc  = std::function<void(const std::string&, V&)>;

    struct Config {
        size_t maxItems      = 1000;                        // 每个分片的最大条目数
        size_t numShards     = 20;                          // 分片数量 (固定)
        std::chrono::seconds ttl{300};                    // 默认过期时间
        bool blockOnExpire   = false;                       // true=阻塞等待刷新; false=返回旧值+异步刷新
    };

    // 工厂方法 — 必须通过此方式构造 (保证 shared_from_this 可用)
    static std::shared_ptr<LruCache> create(Config cfg, LoadFunc loader, EvictFunc evictor) {
        auto ptr = std::shared_ptr<LruCache>(new LruCache(std::move(cfg), std::move(loader), std::move(evictor)));
        ptr->startRefreshThread();
        return ptr;
    }

    ~LruCache() { stopRefreshThread(); }

    LruCache(const LruCache&) = delete;
    LruCache& operator=(const LruCache&) = delete;

    //=== 核心接口 ============================================================

    /** 获取缓存值, 不存在或过期(且刷新失败)返回 std::nullopt */
    std::optional<V> get(const std::string& key);

    /** 写入/覆盖缓存 */
    void set(const std::string& key, V value);

    /** 删除指定 key */
    void remove(const std::string& key);

    /** 清空所有分片 */
    void clear();

    /** 获取统计信息 */
    [[nodiscard]] const LruStats& stats() const { return stats_; }
    [[nodiscard]] LruStats&       stats()       { return stats_; }

private:
    //=== 内部数据结构 ========================================================

    struct Node {
        std::string key;
        V           value;
        std::chrono::steady_clock::time_point expireAt;
    };

    struct Shard {
        mutable std::mutex mtx;
        std::list<Node>    order;  // front = MRU, back = LRU
        std::unordered_map<std::string, typename std::list<Node>::iterator> index;
    };

    struct SfEntry {
        std::mutex              mtx;
        std::condition_variable cv;
        bool                    done = false;
    };

    struct RefreshTask {
        std::string key;
        std::weak_ptr<LruCache> cache;
    };

    //=== 私有构造 (由 create 调用) ===========================================
    explicit LruCache(Config cfg, LoadFunc loader, EvictFunc evictor)
        : config_(std::move(cfg))
        , loader_(std::move(loader))
        , evictor_(std::move(evictor))
        , shards_(config_.numShards)
    {
        for (auto& s : shards_) s = std::make_unique<Shard>();
    }

    //=== 分片操作 ============================================================
    Shard& shardOf(const std::string& key) {
        // FNV-1a hash: 分布均匀, 速度快
        uint64_t h = 14695981039346656037ull;
        for (char c : key) {
            h ^= static_cast<uint64_t>(c);
            h *= 1099511628211ull;
        }
        return *shards_[h % config_.numShards];
    }

    void putInternal(Shard& shard, const std::string& key, V value);
    void evictIfNeeded(Shard& shard);

    //=== Singleflight ========================================================
    std::optional<V> getWithSingleflight(const std::string& key);

    //=== 异步刷新线程 ========================================================
    void startRefreshThread();
    void stopRefreshThread();
    void refreshThreadFunc();
    void refreshOne(const std::string& key, std::shared_ptr<LruCache> self);

    //=== 成员变量 ============================================================
    Config config_;
    LoadFunc loader_;
    EvictFunc evictor_;
    std::vector<std::unique_ptr<Shard>> shards_;

    // singleflight
    std::mutex sfMtx_;
    std::unordered_map<std::string, std::shared_ptr<SfEntry>> sfMap_;

    // 异步刷新
    std::thread                     refreshThread_;
    std::mutex                      refreshMtx_;
    std::condition_variable         refreshCv_;
    std::queue<RefreshTask>         refreshQueue_;
    std::atomic<bool>               running_{false};

    // 失败计数
    std::mutex                      failMtx_;
    std::unordered_map<std::string, uint32_t> failCounts_;

    LruStats stats_;
};

//==============================================================================
// 实现
//==============================================================================

template<typename V>
void LruCache<V>::putInternal(Shard& shard, const std::string& key, V value) {
    auto it = shard.index.find(key);
    stats_.recordSyncInput();
    if (it != shard.index.end()) {
        // 已存在: 更新值, 移到队首
        it->second->value = std::move(value);
        it->second->expireAt = std::chrono::steady_clock::now() + config_.ttl;
        shard.order.splice(shard.order.begin(), shard.order, it->second);
    } else {
        // 新节点: 插入队首
        shard.order.push_front(Node{key, std::move(value),
                                    std::chrono::steady_clock::now() + config_.ttl});
        shard.index[key] = shard.order.begin();
        evictIfNeeded(shard);
    }
}

template<typename V>
void LruCache<V>::evictIfNeeded(Shard& shard) {
    while (shard.index.size() > config_.maxItems) {
        auto& lru = shard.order.back();
        if (evictor_) evictor_(lru.key, lru.value);
        shard.index.erase(lru.key);
        shard.order.pop_back();
        stats_.recordEviction();
    }
}

template<typename V>
std::optional<V> LruCache<V>::get(const std::string& key) {
    Shard& shard = shardOf(key);
    {
        std::lock_guard<std::mutex> lk(shard.mtx);
        auto it = shard.index.find(key);
        if (it != shard.index.end()) {
            // 命中 — 检查是否过期
            auto now = std::chrono::steady_clock::now();
            if (it->second->expireAt > now) {
                // 未过期: 移到队首, 返回
                shard.order.splice(shard.order.begin(), shard.order, it->second);
                stats_.recordHit();
                return it->second->value;
            }
            // 已过期: 逻辑过期处理
            if (config_.blockOnExpire) {
                // 阻塞模式: 释放分片锁后由 singleflight 处理
                goto MISS;
            } else {
                // 异步模式: 返回旧值 + 派发刷新
                V oldVal = it->second->value;
                shard.order.splice(shard.order.begin(), shard.order, it->second);
                stats_.recordHit();
                {
                    std::lock_guard<std::mutex> qlk(refreshMtx_);
                    refreshQueue_.push({key, this->shared_from_this()});
                }
                refreshCv_.notify_one();
                return oldVal;
            }
        }
    }

MISS:
    // miss 或过期阻塞模式
    stats_.recordMiss();
    return getWithSingleflight(key);
}

template<typename V>
std::optional<V> LruCache<V>::getWithSingleflight(const std::string& key) {
    std::shared_ptr<SfEntry> entry;
    bool isFirst = false;

    // 1. 查找或创建 singleflight 条目
    {
        std::lock_guard<std::mutex> lk(sfMtx_);
        auto it = sfMap_.find(key);
        if (it != sfMap_.end()) {
            entry = it->second;
            isFirst = false;
        } else {
            entry = std::make_shared<SfEntry>();
            sfMap_[key] = entry;
            isFirst = true;
        }
    }

    if (!isFirst) {
        // 2a. 非首个线程: 等待刷新完成
        std::unique_lock<std::mutex> lk(entry->mtx);
        entry->cv.wait(lk, [&] { return entry->done; });
        stats_.recordSfMerge();
        // 重新从 LRU 获取
        return get(key);
    }

    // 2b. 首个线程: 拉取数据
    auto loaded = loader_(key);

    // 3. 插入 LRU (持分片锁)
    if (loaded.has_value()) {
        Shard& shard = shardOf(key);
        std::lock_guard<std::mutex> lk(shard.mtx);
        putInternal(shard, key, std::move(loaded.value()));
    }

    // 4. 通知等待线程 + 清理 singleflight
    {
        std::lock_guard<std::mutex> lk(sfMtx_);
        {
            std::lock_guard<std::mutex> elk(entry->mtx);
            entry->done = true;
        }
        entry->cv.notify_all();
        sfMap_.erase(key);
    }

    // 5. 返回
    if (loaded.has_value()) {
        return loaded.value();
    }
    return std::nullopt;
}

template<typename V>
void LruCache<V>::set(const std::string& key, V value) {
    Shard& shard = shardOf(key);
    std::lock_guard<std::mutex> lk(shard.mtx);
    putInternal(shard, key, std::move(value));
}

template<typename V>
void LruCache<V>::remove(const std::string& key) {
    Shard& shard = shardOf(key);
    std::lock_guard<std::mutex> lk(shard.mtx);
    auto it = shard.index.find(key);
    if (it != shard.index.end()) {
        shard.order.erase(it->second);
        shard.index.erase(it);
    }
}

template<typename V>
void LruCache<V>::clear() {
    for (auto& s : shards_) {
        std::lock_guard<std::mutex> lk(s->mtx);
        s->order.clear();
        s->index.clear();
    }
}

template<typename V>
void LruCache<V>::startRefreshThread() {
    running_ = true;
    refreshThread_ = std::thread([this] { refreshThreadFunc(); });
}

template<typename V>
void LruCache<V>::stopRefreshThread() {
    running_ = false;
    refreshCv_.notify_all();
    if (refreshThread_.joinable()) {
        refreshThread_.join();
    }
}

template<typename V>
void LruCache<V>::refreshThreadFunc() {
    while (running_) {
        RefreshTask task;
        {
            std::unique_lock<std::mutex> lk(refreshMtx_);
            refreshCv_.wait(lk, [&] { return !refreshQueue_.empty() || !running_; });
            if (!running_) break;
            task = std::move(refreshQueue_.front());
            refreshQueue_.pop();
        }
        if (auto self = task.cache.lock()) {
            refreshOne(task.key, std::move(self));
        }
        // weak_ptr 已过期 → 缓存已销毁, 跳过
    }
}

template<typename V>
void LruCache<V>::refreshOne(const std::string& key, std::shared_ptr<LruCache> self) {
    stats_.recordRefresh();
    auto loaded = loader_(key);

    Shard& shard = shardOf(key);
    std::lock_guard<std::mutex> lk(shard.mtx);

    if (!loaded.has_value()) {
        // 刷新失败: 分级处理
        stats_.recordRefreshFail();
        uint32_t fails = 0;
        {
            std::lock_guard<std::mutex> flk(failMtx_);
            auto it = failCounts_.find(key);
            if (it != failCounts_.end()) {
                fails = ++it->second;
            } else {
                failCounts_[key] = fails = 1;
            }
        }
        // 数据不存在 (loader 返回 nullopt) 或连续 5 次失败 → 删除
        if (fails >= 5) {
            auto it = shard.index.find(key);
            if (it != shard.index.end()) {
                if (evictor_) evictor_(key, it->second->value);
                shard.order.erase(it->second);
                shard.index.erase(it);
                stats_.recordEviction();
            }
            std::lock_guard<std::mutex> flk(failMtx_);
            failCounts_.erase(key);
        }
        // 否则保留旧值, 下次命中再尝试
        return;
    }

    // 刷新成功: 重置失败计数, 更新缓存
    {
        std::lock_guard<std::mutex> flk(failMtx_);
        failCounts_.erase(key);
    }
    putInternal(shard, key, std::move(loaded.value()));
}

#endif //IMSERVER_LRU_CACHE_H
