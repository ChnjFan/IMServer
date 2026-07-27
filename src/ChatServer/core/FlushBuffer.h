//
// Created by Fan on 2026/07/25.
//

#ifndef IMSERVER_FLUSH_BUFFER_H
#define IMSERVER_FLUSH_BUFFER_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <vector>

#include "ChatMsgNode.h"

/**
 * @brief 双缓冲区：IO 线程 push 到 active，DB 线程 swap 后消费 flush。
 *
 * 使用 std::vector<shared_ptr<ChatMsgNode>> + shared_mutex 实现。
 * push 持 shared_lock（多 IO 线程并发），swap 持 unique_lock（独占）。
 */
class FlushBuffer {
public:
    FlushBuffer() : active_idx_(0) {
        buffers_[0].reserve(256);
        buffers_[1].reserve(256);
    }

    // IO 线程调用：推入活跃缓冲区
    void push(std::shared_ptr<ChatMsgNode> node) {
        std::shared_lock lk(mtx_);
        int idx = active_idx_.load(std::memory_order_relaxed);
        buffers_[idx].push_back(std::move(node));
    }

    // 定时器线程调用：检查活跃缓冲区是否有数据（无锁，允许误判）
    [[nodiscard]] bool hasData() const {
        return !buffers_[active_idx_.load(std::memory_order_relaxed)].empty();
    }

    // DB 写入线程调用：交换缓冲区，返回待刷写的内容
    std::vector<std::shared_ptr<ChatMsgNode>> swap() {
        std::unique_lock lk(mtx_);
        int old = active_idx_.load(std::memory_order_relaxed);
        active_idx_.store(1 - old, std::memory_order_relaxed);
        // 交换后，buffers_[old] 是待刷写的，buffers_[1-old] 是新的活跃缓冲区
        // 但我们需要返回 old 的内容，并清空它供下一轮使用
        std::vector<std::shared_ptr<ChatMsgNode>> result;
        result.swap(buffers_[old]);
        last_swap_time_us_.store(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count(),
            std::memory_order_relaxed);
        return result;
    }

    /// 上次 swap 的时间戳 (μs)
    [[nodiscard]] int64_t lastSwapTimeUs() const { return last_swap_time_us_.load(std::memory_order_relaxed); }

private:
    std::vector<std::shared_ptr<ChatMsgNode>> buffers_[2];
    std::atomic<int> active_idx_;
    std::atomic<int64_t> last_swap_time_us_{0};
    mutable std::shared_mutex mtx_;  // push=shared, swap=unique
};

#endif //IMSERVER_FLUSH_BUFFER_H
