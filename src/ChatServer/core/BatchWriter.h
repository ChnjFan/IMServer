//
// Created by Fan on 2026/07/25.
//

#ifndef IMSERVER_BATCH_WRITER_H
#define IMSERVER_BATCH_WRITER_H

#include <thread>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

#include <boost/lockfree/queue.hpp>

#include "FlushBuffer.h"

/**
 * @brief 聊天消息批量异步写入管理器。
 *
 * 架构：
 *   - 全局定时器 (200ms): 轮询所有 shard，将有数据的 shard 索引推入 task queue
 *   - DB 写入线程池 (shard_count/4): 从 task queue 取 shard 索引，swap 缓冲区，批量写 MySQL
 *
 * 监控 (Metrics):
 *   - flush/s              : 每秒刷写次数
 *   - msg/s                : 每秒写入消息数
 *   - avg_latency_us       : 单次刷写耗时
 *   - avg_batch            : 每批消息数
 *   - avg_queue_wait_us    : 消息排队等待时间
 *   - dead_letter_count    : 死信消息数
 */
class BatchWriter {
public:
    explicit BatchWriter(size_t num_shards, size_t num_writers);
    ~BatchWriter();

    void start();
    void stop();

    FlushBuffer* bufferAt(size_t shard_idx) { return buffers_[shard_idx].get(); }

    //=== 监控指标 ============================================================
    void printMetrics();

private:
    //=== 线程函数 ============================================================
    void timerLoop();
    void writerLoop();

    //=== 核心逻辑 ============================================================
    void flushShard(size_t shard_idx);
    void flushBatch(std::vector<std::shared_ptr<ChatMsgNode>>& nodes);
    void handleFailed(std::vector<std::shared_ptr<ChatMsgNode>> failed);

    //=== 数据结构 ============================================================
    std::vector<std::unique_ptr<FlushBuffer>> buffers_;
    boost::lockfree::queue<size_t> task_queue_{4096};

    std::atomic<bool> running_{false};
    std::thread timer_thread_;
    size_t num_writers_ = 0;
    std::vector<std::thread> writers_;

    // 死信队列
    std::mutex dlm_mtx_;
    std::vector<std::shared_ptr<ChatMsgNode>> dead_letter_queue_;

    //=== 监控指标 ============================================================
    struct alignas(64) Metrics {
        std::atomic<uint64_t> flush_count{0};
        std::atomic<uint64_t> flush_latency_us{0};
        std::atomic<uint64_t> flush_msg_count{0};
        std::atomic<uint64_t> dead_letter_count{0};
        std::atomic<uint64_t> node_lifetime_us{0};
        std::atomic<uint64_t> node_lifetime_count=0;
    } metrics_;

    std::chrono::steady_clock::time_point last_metric_time_;
};

#endif //IMSERVER_BATCH_WRITER_H
