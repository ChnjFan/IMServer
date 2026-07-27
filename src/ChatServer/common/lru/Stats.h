//
// Created by Fan on 2026/07/24.
//

#ifndef IMSERVER_LRU_STATS_H
#define IMSERVER_LRU_STATS_H

#include <atomic>
#include <cstdint>

/**
 * LRU 缓存统计指标（无锁，基于 atomic）
 * 独立封装，便于后续扩展或替换为带回调的监控
 */
class LruStats {
public:
    void recordHit()              { hits_.fetch_add(1, std::memory_order_relaxed); }
    void recordMiss()             { misses_.fetch_add(1, std::memory_order_relaxed); }
    void recordEviction()         { evictions_.fetch_add(1, std::memory_order_relaxed); }
    void recordSfMerge()          { sfMerges_.fetch_add(1, std::memory_order_relaxed); }
    void recordSyncInput()        { syncInput.fetch_add(1, std::memory_order_relaxed); }
    void recordRefresh()          { refreshes_.fetch_add(1, std::memory_order_relaxed); }
    void recordRefreshFail()      { refreshFails_.fetch_add(1, std::memory_order_relaxed); }

    [[nodiscard]] uint64_t hits()            const { return hits_.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64_t misses()          const { return misses_.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64_t evictions()       const { return evictions_.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64_t sfMerges()        const { return sfMerges_.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64_t syncInputs()      const { return syncInput.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64_t refreshes()       const { return refreshes_.load(std::memory_order_relaxed); }
    [[nodiscard]] uint64_t refreshFails()    const { return refreshFails_.load(std::memory_order_relaxed); }

    [[nodiscard]] uint64_t total() const {
        return hits_.load(std::memory_order_relaxed) + misses_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] double hitRate() const {
        auto t = total();
        return t > 0 ? static_cast<double>(hits_.load(std::memory_order_relaxed)) / t : 0.0;
    }

private:
    std::atomic<uint64_t> hits_{0};
    std::atomic<uint64_t> misses_{0};
    std::atomic<uint64_t> evictions_{0};
    std::atomic<uint64_t> sfMerges_{0};       // singleflight 合并次数
    std::atomic<uint64_t> syncInput{0};       // 同步更新次数
    std::atomic<uint64_t> refreshes_{0};      // 异步刷新次数
    std::atomic<uint64_t> refreshFails_{0};   // 异步刷新失败次数
};

#endif //IMSERVER_LRU_STATS_H
