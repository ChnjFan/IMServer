#ifndef IMSERVER_STRESS_METRICS_H
#define IMSERVER_STRESS_METRICS_H

#include <atomic>
#include <cstdint>
#include "metrics.h"

/**
 * @brief 压力测试专用指标
 */
class StressMetrics {
public:
    StressMetrics();

    // === 连接指标 ===
    std::atomic<uint64_t> connect_attempts{0};
    std::atomic<uint64_t> connect_success{0};
    std::atomic<uint64_t> connect_failed{0};
    std::atomic<uint64_t> connect_timeout{0};
    std::atomic<uint64_t> handshake_success{0};
    std::atomic<uint64_t> handshake_failed{0};

    // === 消息指标 ===
    std::atomic<uint64_t> msg_sent{0};
    std::atomic<uint64_t> msg_recv{0};

    // === 混合消息 per-type 指标 ===
    std::atomic<uint64_t> chat_msg_sent{0};
    std::atomic<uint64_t> chat_msg_recv{0};
    std::atomic<uint64_t> friend_apply_sent{0};
    std::atomic<uint64_t> friend_apply_recv{0};
    std::atomic<uint64_t> user_search_sent{0};
    std::atomic<uint64_t> user_search_recv{0};

    // === 连接维持 ===
    std::atomic<uint64_t> disconnect_{0};
    std::atomic<int64_t> current_online{0};
    std::atomic<int64_t> peak_online{0};

    // === 延迟 ===
    LatencyHistogram rtt_hist;

    // === 工具 ===
    // 断线率 = 断线数 / (连接成功 + 断线)
    // 适用于所有场景：Burst/Sustained 应接近 0，Mixed 会较高（预期行为）
    double errorRate() const {
        uint64_t connected = connect_success.load();
        uint64_t disconnected = disconnect_.load();
        uint64_t total = connected + disconnected;
        return total == 0 ? 0.0 : static_cast<double>(disconnected) / total;
    }
};

#endif // IMSERVER_STRESS_METRICS_H
