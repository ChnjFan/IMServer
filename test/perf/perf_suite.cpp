#include "perf_suite.h"
#include "chat_test_client.h"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <iostream>
#include <algorithm>

namespace {
std::atomic<bool> gStop{false};

// 客户端工作循环：混合 70% 消息 + 20% 心跳 + 10% 行为切换
static void mixedClient(int idx, const PerfSuite::Config& cfg,
                       Metrics& metrics, std::atomic<bool>& stop) {
    ChatTestClient client;
    if (!client.connect(cfg.chatHost, cfg.chatPort)) {
        return;
    }
    // 先登录（uid 用 idx 避免冲突）
    client.chatLogin(5000 + idx, "perf_token");

    int seq = 0;
    while (!stop.load()) {
        auto start = std::chrono::steady_clock::now();
        bool ok = false;

        int action = seq++ % 10;
        if (action < 7) {
            ok = client.sendChatMsg(0, "perf msg");       // 70% 消息
        } else if (action < 9) {
            ok = client.heartbeat();                       // 20% 心跳
        } else {
            client.disconnect();                           // 10% 重连
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            ok = client.connect(cfg.chatHost, cfg.chatPort);
        }

        auto end = std::chrono::steady_clock::now();
        metrics.throughput.tick();
        metrics.latency.record(
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
        if (ok) metrics.errors.addSuccess(); else metrics.errors.addFailed();
    }
}

// 纯消息客户端（用于阶梯/极限施压）
static void messageClient(int idx, const PerfSuite::Config& cfg,
                         Metrics& metrics, std::atomic<bool>& stop) {
    ChatTestClient client;
    if (!client.connect(cfg.chatHost, cfg.chatPort)) {
        return;
    }
    client.chatLogin(5000 + idx, "perf_token");

    while (!stop.load()) {
        auto start = std::chrono::steady_clock::now();
        bool ok = client.sendChatMsg(0, "perf");
        auto end = std::chrono::steady_clock::now();
        metrics.throughput.tick();
        metrics.latency.record(
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
        if (ok) metrics.errors.addSuccess(); else metrics.errors.addFailed();
    }
}

// 运行指定数量客户端持续 stepSec 秒，返回测量结果
static PerfLevel runLevel(const PerfSuite::Config& cfg, int clientCount,
                          int stepSec, bool mixed) {
    gStop = false;
    Metrics metrics;

    std::vector<std::thread> threads;
    for (int i = 0; i < clientCount; i++) {
        if (mixed) {
            threads.emplace_back(mixedClient, i, std::cref(cfg),
                                 std::ref(metrics), std::ref(gStop));
        } else {
            threads.emplace_back(messageClient, i, std::cref(cfg),
                                 std::ref(metrics), std::ref(gStop));
        }
    }

    std::this_thread::sleep_for(std::chrono::seconds(stepSec));
    gStop = true;
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    PerfLevel lvl;
    lvl.clientCount = clientCount;
    lvl.p50_us = metrics.latency.percentile(0.5);
    lvl.p99_us = metrics.latency.percentile(0.99);
    lvl.errorRate = metrics.errors.errorRate();
    lvl.qps = stepSec > 0
        ? static_cast<double>(metrics.throughput.total()) / stepSec
        : 0;
    return lvl;
}
}  // namespace

PerfSuite::PerfSuite(Config config) : config_(config) {}

std::vector<PerfLevel> PerfSuite::runRampUp() {
    std::vector<PerfLevel> results;
    for (int n : {10, 50, 100, 200, 500}) {
        std::cout << "[perf] ramp-up: " << n << " clients...\n";
        results.push_back(runLevel(config_, n, config_.stepSec, false));
    }
    return results;
}

int PerfSuite::runToBreak() {
    int n = 10;
    while (n <= 2000) {
        std::cout << "[perf] to-break: " << n << " clients...\n";
        auto lvl = runLevel(config_, n, config_.stepSec, false);
        if (lvl.errorRate > 0.05) {
            return n;  // 崩溃点
        }
        n *= 2;
    }
    return n;
}

PerfLevel PerfSuite::runMixedWorkload(int clientCount) {
    std::cout << "[perf] mixed workload: " << clientCount << " clients...\n";
    return runLevel(config_, clientCount, config_.stepSec, true);
}
