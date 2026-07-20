#include "perf_suite.h"
#include "chat_test_client.h"
#include "perf_user_setup.h"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <iostream>
#include <algorithm>

#include "AsioIOServicePool.h"

namespace {
std::atomic<bool> gStop{false};

// 客户端工作循环：混合 70% 消息 + 20% 心跳 + 10% 行为切换
// static void mixedClient(int idx, const PerfSuite::Config& cfg,
//                        Metrics& metrics, std::atomic<bool>& stop) {
//     ChatTestClient client;
//     // 注册测试用户 → 从 StatusServer 获取真实 token → 登录
//     if (!loginWithRealToken(client, "perf")) {
//         std::cerr << "[perf] mixed client " << idx << " login failed\n";
//         return;
//     }
//
//     int seq = 0;
//     while (!stop.load()) {
//         auto start = std::chrono::steady_clock::now();
//         bool ok = false;
//
//         int action = seq++ % 10;
//         if (action < 7) {
//             ok = client.sendChatMsg(0, "perf msg");       // 70% 消息
//         } else if (action < 9) {
//             ok = client.heartbeat();                       // 20% 心跳
//         } else {
//             client.disconnect();                           // 10% 重连
//             std::this_thread::sleep_for(std::chrono::milliseconds(50));
//             ok = client.connect(cfg.chatHost, cfg.chatPort);
//         }
//
//         auto end = std::chrono::steady_clock::now();
//         metrics.throughput.tick();
//         metrics.latency.record(
//             std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
//         if (ok) metrics.errors.addSuccess(); else metrics.errors.addFailed();
//     }
// }
//

void startMixClients(std::shared_ptr<Metrics> metrics, int clientCount) {
    for (int i = 0; i < clientCount; ++i) {
        auto& io_context = AsioIOServicePool::getInstance()->getIOService();
        auto client = std::make_shared<ChatTestClient>(io_context, [metrics](std::shared_ptr<ChatTestClient> client) {
            auto start = std::chrono::steady_clock::now();
            int action = client->messagesSent() % 10;
            if (action < 7) {
                client->sendChatMsg(0, "perf msg");       // 70% 消息
            } else if (action < 9) {
                client->heartbeat();                       // 20% 心跳
            } else {
                client->close();                           // 10% 重连
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                loginWithRealToken(client, "perf");
            }
            auto end = std::chrono::steady_clock::now();
            metrics->throughput.tick();
            metrics->latency.record(
                std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
            metrics->errors.addSuccess();
        });
        loginWithRealToken(client, "perf");
    }
}

void startMessageClients(std::shared_ptr<Metrics> metrics, int clientCount) {
    for (int i = 0; i < clientCount; ++i) {
        auto& io_context = AsioIOServicePool::getInstance()->getIOService();
        auto client = std::make_shared<ChatTestClient>(io_context, [metrics](std::shared_ptr<ChatTestClient> client) {
            auto start = std::chrono::steady_clock::now();
            client->sendChatMsg(0, "perf");
            auto end = std::chrono::steady_clock::now();
            metrics->throughput.tick();
            metrics->latency.record(
                std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
            metrics->errors.addSuccess();
        });
        loginWithRealToken(client, "perf");
    }
}

// 运行指定数量客户端持续 stepSec 秒，返回测量结果
static PerfLevel runLevel(int clientCount, int stepSec, bool mixed) {
    std::shared_ptr<Metrics> metrics = std::make_shared<Metrics>();

    if (mixed) {
        startMixClients(metrics, clientCount);
    }
    else {
        startMessageClients(metrics, clientCount);
    }

    std::this_thread::sleep_for(std::chrono::seconds(stepSec));

    PerfLevel lvl;
    lvl.clientCount = clientCount;
    lvl.p50_us = metrics->latency.percentile(0.5);
    lvl.p99_us = metrics->latency.percentile(0.99);
    lvl.errorRate = metrics->errors.errorRate();
    lvl.qps = stepSec > 0
        ? static_cast<double>(metrics->throughput.total()) / stepSec
        : 0;
    // 重置指标，避免影响下一轮测试
    metrics->reset();
    return lvl;
}
}  // namespace

PerfSuite::PerfSuite(Config config) : config_(config) {}

std::vector<PerfLevel> PerfSuite::runRampUp() {
    std::vector<PerfLevel> results;
    auto pool = AsioIOServicePool::getInstance();
    boost::asio::io_context io_context{1};
    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&io_context, &pool](const boost::system::error_code &error, int signal_number) {
        if (error) {
            return;
        }
        boost::ignore_unused(signal_number);
        io_context.stop();
        pool->stop();
    });

    for (int n : {10, 50, 100, 200, 500, 700, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000}) {
        std::cout << "[perf] ramp-up: " << n << " clients...\n";
        results.push_back(runLevel(n, config_.stepSec, false));
    }

    return results;
}

int PerfSuite::runToBreak() {
    int n = 10;
    while (n <= 2000) {
        std::cout << "[perf] to-break: " << n << " clients...\n";
        auto lvl = runLevel(n, config_.stepSec, false);
        if (lvl.errorRate > 0.05) {
            return n;  // 崩溃点
        }
        n *= 2;
    }
    return n;
}

PerfLevel PerfSuite::runMixedWorkload(int clientCount) {
    std::cout << "[perf] mixed workload: " << clientCount << " clients...\n";
    return runLevel(clientCount, config_.stepSec, true);
}
