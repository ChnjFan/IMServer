#include <gtest/gtest.h>

#include "stress_fixture.h"
#include "stress_connection_pool.h"
#include "report_output.h"

#include <chrono>
#include <iostream>
#include <iomanip>
#include <thread>
#include <random>

using namespace std::chrono_literals;

/**
 * @brief 场景 4: 混合真实场景
 *
 * 测试目标: 模拟真实用户上下线行为
 * 参数: 1000 基础在线 + 50 conn/s 涌入/涌出 + 10% 用户发消息
 * 持续: 5min (1000 + 50*5*60 = 16000 → 适配 12000 需要调整)
 *
 * 调整: BASE=500, CHURN_RATE=20, DURATION=5min
 *       总账号 = 500 + 20*5*60 = 6500
 */

class MixedScenarioTest : public StressTestFixture {
protected:
    static constexpr int IO_CONTEXT_NUM = 8;
    static constexpr int BASE_CONNECTIONS = 500;
    static constexpr int CHURN_RATE = 20;  // 每秒涌入/涌出 20
    static constexpr int TEST_DURATION_MINUTES = 5;
};

TEST_F(MixedScenarioTest, Churn_5min) {
    // 计算所需账号
    int totalNeeded = BASE_CONNECTIONS + CHURN_RATE * TEST_DURATION_MINUTES * 60;
    auto accounts = takeAccounts(totalNeeded);
    ASSERT_GE(static_cast<int>(accounts.size()), BASE_CONNECTIONS);

    if (static_cast<int>(accounts.size()) < totalNeeded) {
        std::cout << "[MixedScenario] Warning: only " << accounts.size()
                  << " accounts available" << std::endl;
    }

    int ioCount = std::max(4, static_cast<int>(std::thread::hardware_concurrency()) - 2);
    StressConnectionPool pool(ioCount);
    ReportOutput report("MixedScenario");

    // 初始连接
    int actualBase = std::min(BASE_CONNECTIONS, static_cast<int>(accounts.size()));
    auto initial = std::vector<TestAccount>(accounts.begin(), accounts.begin() + actualBase);
    pool.addAndConnect(initial, 100, 200ms);

    auto deadline = std::chrono::steady_clock::now() + 30s;
    while (pool.onlineCount() < actualBase * 0.9) {
        if (std::chrono::steady_clock::now() > deadline) break;
        std::this_thread::sleep_for(200ms);
    }

    std::cout << "\n=== MixedScenario: " << pool.onlineCount()
              << " base connections ===" << std::endl;

    // 上下线模拟
    std::atomic<bool> stopChurn(false);
    std::thread churnThread([&] {
        int accountCursor = actualBase;
        auto nextTick = std::chrono::steady_clock::now();

        while (!stopChurn.load()) {
            nextTick += 1s;
            std::this_thread::sleep_until(nextTick);
            if (stopChurn.load()) break;

            pool.disconnectRandom(CHURN_RATE);

            if (accountCursor + CHURN_RATE <= static_cast<int>(accounts.size())) {
                auto batch = std::vector<TestAccount>(
                    accounts.begin() + accountCursor,
                    accounts.begin() + accountCursor + CHURN_RATE);
                accountCursor += CHURN_RATE;
                pool.addAndConnect(batch, 50, 100ms);
            }
        }
    });

    // 消息模拟
    std::atomic<bool> stopChat(false);
    std::thread chatThread([&] {
        std::random_device rd;
        std::mt19937 gen(rd());

        while (!stopChat.load()) {
            std::this_thread::sleep_for(1s);
            if (stopChat.load()) break;

            auto sample = pool.sampleOnlineClients(
                std::max(1, static_cast<int>(pool.onlineCount() * 0.1)));
            for (auto& c : sample) {
                c->sendChatMsg(c->uid() + 1, "mixed scenario message");
            }
        }
    });

    // 监控
    std::atomic<bool> stopMonitor(false);
    std::thread monitor([&] {
        int t = 0;
        while (!stopMonitor.load()) {
            std::this_thread::sleep_for(10s);
            if (stopMonitor.load()) break;
            t += 10;
            report.tick(pool.metrics(), pool.onlineCount(), t);
            std::cout << "[t=" << std::setw(4) << t << "s] online=" << pool.onlineCount()
                      << " total=" << pool.totalClients()
                      << " disc=" << pool.metrics().disconnect_.load()
                      << std::endl;
        }
    });

    std::cout << "Running mixed scenario for " << TEST_DURATION_MINUTES << " minutes..." << std::endl;
    std::this_thread::sleep_for(std::chrono::minutes(TEST_DURATION_MINUTES));

    stopChurn.store(true);
    stopChat.store(true);
    stopMonitor.store(true);
    churnThread.join();
    chatThread.join();
    monitor.join();

    auto& m = pool.metrics();

    report.summary(m, BASE_CONNECTIONS, TEST_DURATION_MINUTES * 60 * 1000);
    report.saveCsv("mixed_scenario_report.csv");

    std::cout << "\n=== MixedScenario Results ===" << std::endl;
    std::cout << "Final online:  " << pool.onlineCount() << std::endl;
    std::cout << "Total clients: " << pool.totalClients() << std::endl;
    std::cout << "Disconnects:   " << m.disconnect_.load() << std::endl;
    std::cout << "==============================\n" << std::endl;

    EXPECT_GE(pool.onlineCount(), 200);

    pool.gracefulShutdown();
}
