#include "stability_runner.h"
#include "chat_test_client.h"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <iostream>
#include <algorithm>

namespace {
std::atomic<bool> gStop{false};
}

StabilityRunner::StabilityRunner(StabilityConfig config) : config_(config) {}

// 在一个客户端上持续发送心跳，直到 stop 或断连
static void keepAliveClient(int idx, const StabilityConfig& cfg,
                            Metrics& metrics, std::atomic<bool>& stop) {
    ChatTestClient client;
    if (!client.connect(cfg.chatHost, cfg.chatPort)) {
        std::cerr << "[keepalive] client " << idx << " connect failed\n";
        return;
    }

    while (!stop.load()) {
        auto start = std::chrono::steady_clock::now();
        client.heartbeat();
        auto end = std::chrono::steady_clock::now();

        metrics.throughput.tick();
        if (client.isConnected()) {
            metrics.errors.addSuccess();
        } else {
            metrics.errors.addFailed();
            break;
        }
        metrics.latency.record(
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());

        std::this_thread::sleep_for(cfg.heartbeatInterval);
    }
}

// 反复上下线
static void churnClient(int idx, const StabilityConfig& cfg,
                        Metrics& metrics, std::atomic<bool>& stop) {
    while (!stop.load()) {
        ChatTestClient client;
        if (!client.connect(cfg.chatHost, cfg.chatPort)) {
            metrics.errors.addFailed();
            std::this_thread::sleep_for(cfg.offlineSec);
            continue;
        }
        metrics.throughput.tick();
        metrics.errors.addSuccess();

        std::this_thread::sleep_for(cfg.onlineSec);
        client.disconnect();
        metrics.throughput.tick();

        std::this_thread::sleep_for(cfg.offlineSec);
    }
}

// 持续发消息
static void messageStormClient(int idx, const StabilityConfig& cfg,
                               Metrics& metrics, std::atomic<bool>& stop) {
    ChatTestClient client;
    if (!client.connect(cfg.chatHost, cfg.chatPort)) {
        std::cerr << "[storm] client " << idx << " connect failed\n";
        return;
    }

    int seq = 0;
    auto interval = std::chrono::microseconds(1000000 / std::max(1, cfg.msgPerClientPerSec));

    while (!stop.load()) {
        auto start = std::chrono::steady_clock::now();
        client.sendChatMsg(0, "storm msg " + std::to_string(seq++));
        auto end = std::chrono::steady_clock::now();

        metrics.throughput.tick();
        metrics.latency.record(
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());

        if (client.isConnected()) {
            metrics.errors.addSuccess();
        } else {
            metrics.errors.addFailed();
            break;
        }
        std::this_thread::sleep_until(start + interval);
    }
}

// 运行单一场景，返回是否满足通过标准
bool StabilityRunner::runScenario(
    void (*fn)(int, const StabilityConfig&, Metrics&, std::atomic<bool>&)) {
    gStop = false;
    metrics_.reset();
    report_ = ReportWriter();

    try {
        auto baseline = resourceMon_.sample();

        std::vector<std::thread> threads;
        for (int i = 0; i < config_.clientCount; i++) {
            threads.emplace_back(fn, i, std::cref(config_),
                                 std::ref(metrics_), std::ref(gStop));
        }

        auto start = std::chrono::steady_clock::now();
        while (!gStop.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            report_.appendSample(now, metrics_);

            auto current = resourceMon_.sample();
            if (resourceMon_.isLeaking(baseline, current)) {
                std::cerr << "[stability] resource leak detected, stopping\n";
                break;
            }

            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed >= config_.durationSec) break;
        }

        gStop = true;
        for (auto& t : threads) {
            if (t.joinable()) t.join();
        }
    } catch (std::exception& e) {
        std::cerr << e.what() << "\n";
    }

    // 通过标准：错误率 < 0.1%
    return metrics_.errors.errorRate() < 0.001;
}

bool StabilityRunner::runKeepAlive() {
    return runScenario(keepAliveClient);
}

bool StabilityRunner::runChurn() {
    return runScenario(churnClient);
}

bool StabilityRunner::runMessageStorm() {
    return runScenario(messageStormClient);
}

bool StabilityRunner::runMixed() {
    gStop = false;
    metrics_.reset();
    report_ = ReportWriter();

    auto baseline = resourceMon_.sample();

    std::vector<std::thread> threads;
    for (int i = 0; i < config_.clientCount; i++) {
        // 轮流分配三种行为
        if (i % 3 == 0) {
            threads.emplace_back(keepAliveClient, i, std::cref(config_),
                                 std::ref(metrics_), std::ref(gStop));
        } else if (i % 3 == 1) {
            threads.emplace_back(churnClient, i, std::cref(config_),
                                 std::ref(metrics_), std::ref(gStop));
        } else {
            threads.emplace_back(messageStormClient, i, std::cref(config_),
                                 std::ref(metrics_), std::ref(gStop));
        }
    }

    auto start = std::chrono::steady_clock::now();
    while (!gStop.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        report_.appendSample(now, metrics_);

        auto current = resourceMon_.sample();
        if (resourceMon_.isLeaking(baseline, current)) {
            std::cerr << "[stability] resource leak detected, stopping\n";
            break;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= config_.durationSec) break;
    }

    gStop = true;
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    return metrics_.errors.errorRate() < 0.001;
}
