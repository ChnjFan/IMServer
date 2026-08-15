//
// Created by Fan on 2026/07/25.
//

#include "BatchWriter.h"

#include <chrono>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <unordered_map>

#include "net/Session.h"
#include "db/mysql/MysqlMgr.h"
#include "const.h"

// ──────────────────────────────────────────────────────────────
// Construction / Lifecycle
// ──────────────────────────────────────────────────────────────

BatchWriter::BatchWriter(size_t num_shards, size_t num_writers)
    : buffers_(num_shards)
    , num_writers_(num_writers)
    , last_metric_time_(std::chrono::steady_clock::now())
{
    for (auto& b : buffers_) {
        b = std::make_unique<FlushBuffer>();
    }
}

BatchWriter::~BatchWriter() {
    stop();
}

void BatchWriter::start() {
    running_ = true;
    // 启动写入线程
    for (size_t i = 0; i < num_writers_; i++) {
        writers_.emplace_back([this] { writerLoop(); });
    }
    timer_thread_ = std::thread([this] { timerLoop(); });
}

void BatchWriter::stop() {
    running_ = false;
    if (timer_thread_.joinable()) timer_thread_.join();
    for (auto& w : writers_) {
        if (w.joinable()) w.join();
    }
}

// ──────────────────────────────────────────────────────────────
// Timer Thread
// ──────────────────────────────────────────────────────────────

void BatchWriter::timerLoop() {
    while (running_) {
        size_t flushed = 0;
        for (size_t i = 0; i < buffers_.size(); i++) {
            if (buffers_[i]->hasData()) {
                task_queue_.push(i);
                flushed++;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

// ──────────────────────────────────────────────────────────────
// Writer Thread
// ──────────────────────────────────────────────────────────────

void BatchWriter::writerLoop() {
    while (running_) {
        size_t shard_idx;
        if (task_queue_.pop(shard_idx)) {
            flushShard(shard_idx);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
}

void BatchWriter::flushShard(size_t shard_idx) {
    auto t0 = std::chrono::steady_clock::now();

    // swap 缓冲区，获取待刷写的消息
    auto nodes = buffers_[shard_idx]->swap();
    if (nodes.empty()) return;

    // 批量写入
    try {
        flushBatch(nodes);
    } catch (const std::exception& e) {
        std::cout << "[BatchWriter] flushBatch exception: " << e.what() << std::endl;
        handleFailed(std::move(nodes));
    }

    // 记录耗时
    auto t1 = std::chrono::steady_clock::now();
    uint64_t latency = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    metrics_.flush_latency_us.fetch_add(latency, std::memory_order_relaxed);
    metrics_.flush_count.fetch_add(1, std::memory_order_relaxed);
    metrics_.flush_msg_count.fetch_add(nodes.size(), std::memory_order_relaxed);
}

// ──────────────────────────────────────────────────────────────
// Batch SQL Execution (delegated to MysqlMgr → ConversationDao)
// ──────────────────────────────────────────────────────────────

void BatchWriter::flushBatch(std::vector<std::shared_ptr<ChatMsgNode>>& nodes) {
    auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    // 记录消息排队时间
    for (auto& n : nodes) {
        metrics_.node_lifetime_us.fetch_add(now_us - n->enqueue_time_us, std::memory_order_relaxed);
        metrics_.node_lifetime_count.fetch_add(1, std::memory_order_relaxed);
    }

    // 调用 DAO 层批量写入
    std::unordered_map<int64_t, int> id_mapping;
    if (!MysqlMgr::getInstance()->batchCreateMessages(nodes, id_mapping)) {
        handleFailed(std::move(nodes));
        return;
    }

    // 回推 serverId 映射给客户端
    for (auto& n : nodes) {
        int64_t key = static_cast<int64_t>(std::hash<std::string>{}(n->msg.convId.value_or(""))) * 1000000000LL + n->msg.msgId;
        auto it = id_mapping.find(key);
        if (it == id_mapping.end()) continue;

        if (auto sess = n->sender_session.lock()) {
            Json::Value rsp;
            rsp["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
            rsp["msg_id"] = n->msg.msgId;
            rsp["server_id"] = it->second;
            rsp["conv_id"] = n->msg.convId.value_or("");
            sess->asyncSend(rsp.toStyledString(),
                static_cast<uint16_t>(MessageID::ID_NOTIFY_MSG_RESULT));
        }
    }
}

// ──────────────────────────────────────────────────────────────
// Error Handling
// ──────────────────────────────────────────────────────────────

void BatchWriter::handleFailed(std::vector<std::shared_ptr<ChatMsgNode>> failed) {
    auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    std::lock_guard lk(dlm_mtx_);
    for (auto& n : failed) {
        // 通知客户端写入失败
        if (auto sess = n->sender_session.lock()) {
            Json::Value err;
            err["error"] = static_cast<int32_t>(ErrorCodes::MYSQL_ERROR);
            err["msg_id"] = n->msg.msgId;
            err["conv_id"] = n->msg.convId.value_or("");
            sess->asyncSend(err.toStyledString(),
                static_cast<uint16_t>(MessageID::ID_CHAT_MSG_RSP));
        }
        dead_letter_queue_.push_back(std::move(n));
    }
    metrics_.dead_letter_count.fetch_add(failed.size(), std::memory_order_relaxed);
}

// ──────────────────────────────────────────────────────────────
// Metrics
// ──────────────────────────────────────────────────────────────

void BatchWriter::printMetrics() {
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - last_metric_time_).count();
    last_metric_time_ = now;

    uint64_t fc = metrics_.flush_count.exchange(0, std::memory_order_relaxed);
    uint64_t fl = metrics_.flush_latency_us.exchange(0, std::memory_order_relaxed);
    uint64_t fm = metrics_.flush_msg_count.exchange(0, std::memory_order_relaxed);
    uint64_t nl = metrics_.node_lifetime_us.exchange(0, std::memory_order_relaxed);
    uint64_t nc = metrics_.node_lifetime_count.exchange(0, std::memory_order_relaxed);
    uint64_t dl = metrics_.dead_letter_count.load(std::memory_order_relaxed);

    double avg_latency = fc > 0 ? static_cast<double>(fl) / fc : 0;
    double avg_batch = fc > 0 ? static_cast<double>(fm) / fc : 0;
    double avg_lifetime = nc > 0 ? static_cast<double>(nl) / nc : 0;
    double flush_per_sec = elapsed > 0 ? fc / elapsed : 0;
    double msg_per_sec = elapsed > 0 ? fm / elapsed : 0;

    std::cout << "[batch_metrics] "
              << "flush/s=" << std::fixed << std::setprecision(1) << flush_per_sec
              << " msg/s=" << std::setprecision(0) << msg_per_sec
              << " avg_latency=" << std::setprecision(0) << avg_latency << "us"
              << " avg_batch=" << std::setprecision(1) << avg_batch
              << " avg_queue_wait=" << std::setprecision(0) << avg_lifetime << "us"
              << " dead_letters=" << dl
              << std::endl;
}
