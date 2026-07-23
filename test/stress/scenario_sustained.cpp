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
 * @brief 场景 3: 持续负载测试
 *
 * 测试目标: 验证长时间运行的稳定性
 * 参数: N 连接保活 10min + 1% 用户每 5s 发消息
 * 断言: 存活率 ≥ 95%，错误率 < 5%
 */

class SustainedLoadTest : public StressTestFixture {
protected:
    static constexpr int TEST_DURATION_MINUTES = 10;
    static constexpr double CHAT_RATIO = 0.01;  // 1% 用户发消息
    static constexpr auto CHAT_INTERVAL = 5s;
};

// 1K 持续负载 (基础验证)
TEST_F(SustainedLoadTest, Sustain_1K_10min) {
    constexpr int TARGET = 1000;

    auto accounts = takeAccounts(TARGET);
    ASSERT_GE(static_cast<int>(accounts.size()), TARGET);

    int ioCount = std::max(4, static_cast<int>(std::thread::hardware_concurrency()) - 2);
    StressConnectionPool pool(ioCount);
    ReportOutput report("SustainedLoad_1K");

    pool.addAndConnect(accounts, 100, 200ms);

    auto deadline = std::chrono::steady_clock::now() + 30s;
    while (pool.onlineCount() < TARGET * 0.95) {
        if (std::chrono::steady_clock::now() > deadline) break;
        std::this_thread::sleep_for(200ms);
    }

    std::cout << "\n=== SustainedLoad_1K: " << pool.onlineCount()
              << " connections established ===" << std::endl;

    std::atomic<bool> stopProducing(false);
    std::thread producer([&] {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> uidDist(accounts.front().uid, accounts.back().uid);

        while (!stopProducing.load()) {
            std::this_thread::sleep_for(CHAT_INTERVAL);
            if (stopProducing.load()) break;

            auto sample = pool.sampleOnlineClients(
                std::max(1, static_cast<int>(pool.onlineCount() * CHAT_RATIO)));
            for (auto& c : sample) {
                c->sendChatMsg(uidDist(gen), "stress test message");
            }
        }
    });

    std::atomic<bool> stopMonitoring(false);
    std::thread monitor([&] {
        int t = 0;
        while (!stopMonitoring.load()) {
            std::this_thread::sleep_for(10s);
            if (stopMonitoring.load()) break;
            t += 10;
            report.tick(pool.metrics(), pool.onlineCount(), t);
            auto& m = pool.metrics();
            std::cout << "[t=" << std::setw(4) << t << "s] online=" << pool.onlineCount()
                      << " msg_sent=" << m.msg_sent.load()
                      << " msg_recv=" << m.msg_recv.load()
                      << " rtt_p99=" << m.rtt_hist.percentile(0.99) << "us"
                      << " disc=" << m.disconnect_.load()
                      << std::endl;
        }
    });

    std::cout << "Running sustained load for " << TEST_DURATION_MINUTES << " minutes..." << std::endl;
    std::this_thread::sleep_for(std::chrono::minutes(TEST_DURATION_MINUTES));

    stopProducing.store(true);
    stopMonitoring.store(true);
    producer.join();
    monitor.join();

    auto& m = pool.metrics();
    int finalOnline = pool.onlineCount();

    report.summary(m, TARGET, TEST_DURATION_MINUTES * 60 * 1000);
    report.saveCsv("sustained_load_1k_report.csv");

    std::cout << "\n=== SustainedLoad_1K Results ===" << std::endl;
    std::cout << "Final online:  " << finalOnline << "/" << TARGET << std::endl;
    std::cout << "Survival rate: " << std::fixed << std::setprecision(2)
              << (finalOnline * 100.0 / TARGET) << "%" << std::endl;
    std::cout << "================================\n" << std::endl;

    EXPECT_GE(finalOnline, TARGET * 0.95);
    EXPECT_LT(m.errorRate(), 0.05);

    pool.gracefulShutdown();
}

// 5K 持续负载 (中等压力)
TEST_F(SustainedLoadTest, Sustain_5K_10min) {
    constexpr int TARGET = 5000;

    auto accounts = takeAccounts(TARGET);
    ASSERT_GE(static_cast<int>(accounts.size()), TARGET);

    int ioCount = std::max(4, static_cast<int>(std::thread::hardware_concurrency()) - 2);
    StressConnectionPool pool(ioCount);
    ReportOutput report("SustainedLoad_5K");

    // 500个/批, 200ms间隔 → 2500 conn/s
    pool.addAndConnect(accounts, 500, 200ms);

    auto deadline = std::chrono::steady_clock::now() + 60s;
    while (pool.onlineCount() < TARGET * 0.90) {
        if (std::chrono::steady_clock::now() > deadline) break;
        std::this_thread::sleep_for(500ms);
    }

    std::cout << "\n=== SustainedLoad_5K: " << pool.onlineCount()
              << " connections established ===" << std::endl;

    std::atomic<bool> stopProducing(false);
    std::thread producer([&] {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> uidDist(accounts.front().uid, accounts.back().uid);

        while (!stopProducing.load()) {
            std::this_thread::sleep_for(CHAT_INTERVAL);
            if (stopProducing.load()) break;

            auto sample = pool.sampleOnlineClients(
                std::max(1, static_cast<int>(pool.onlineCount() * CHAT_RATIO)));
            for (auto& c : sample) {
                c->sendChatMsg(uidDist(gen), "stress test message");
            }
        }
    });

    std::atomic<bool> stopMonitoring(false);
    std::thread monitor([&] {
        int t = 0;
        while (!stopMonitoring.load()) {
            std::this_thread::sleep_for(10s);
            if (stopMonitoring.load()) break;
            t += 10;
            report.tick(pool.metrics(), pool.onlineCount(), t);
            auto& m = pool.metrics();
            std::cout << "[t=" << std::setw(4) << t << "s] online=" << pool.onlineCount()
                      << " msg_sent=" << m.msg_sent.load()
                      << " msg_recv=" << m.msg_recv.load()
                      << " rtt_p99=" << m.rtt_hist.percentile(0.99) << "us"
                      << " disc=" << m.disconnect_.load()
                      << std::endl;
        }
    });

    std::cout << "Running sustained load for " << TEST_DURATION_MINUTES << " minutes..." << std::endl;
    std::this_thread::sleep_for(std::chrono::minutes(TEST_DURATION_MINUTES));

    stopProducing.store(true);
    stopMonitoring.store(true);
    producer.join();
    monitor.join();

    auto& m = pool.metrics();
    int finalOnline = pool.onlineCount();

    report.summary(m, TARGET, TEST_DURATION_MINUTES * 60 * 1000);
    report.saveCsv("sustained_load_5k_report.csv");

    std::cout << "\n=== SustainedLoad_5K Results ===" << std::endl;
    std::cout << "Final online:  " << finalOnline << "/" << TARGET << std::endl;
    std::cout << "Survival rate: " << std::fixed << std::setprecision(2)
              << (finalOnline * 100.0 / TARGET) << "%" << std::endl;
    std::cout << "================================\n" << std::endl;

    EXPECT_GE(finalOnline, TARGET * 0.90);
    EXPECT_LT(m.errorRate(), 0.10);

    pool.gracefulShutdown();
}

// 10K 持续负载 (极限稳定性测试)
TEST_F(SustainedLoadTest, Sustain_10K_10min) {
    constexpr int TARGET = 10000;

    auto accounts = takeAccounts(TARGET);
    ASSERT_GE(static_cast<int>(accounts.size()), TARGET);

    int ioCount = std::max(6, static_cast<int>(std::thread::hardware_concurrency()) - 2);
    StressConnectionPool pool(ioCount);
    ReportOutput report("SustainedLoad_10K");

    // 500个/批, 200ms间隔 → 2500 conn/s
    pool.addAndConnect(accounts, 500, 200ms);

    auto deadline = std::chrono::steady_clock::now() + 120s;
    while (pool.onlineCount() < TARGET * 0.85) {
        if (std::chrono::steady_clock::now() > deadline) break;
        std::this_thread::sleep_for(500ms);
    }

    std::cout << "\n=== SustainedLoad_10K: " << pool.onlineCount()
              << " connections established ===" << std::endl;

    std::atomic<bool> stopProducing(false);
    std::thread producer([&] {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> uidDist(accounts.front().uid, accounts.back().uid);

        while (!stopProducing.load()) {
            std::this_thread::sleep_for(CHAT_INTERVAL);
            if (stopProducing.load()) break;

            auto sample = pool.sampleOnlineClients(
                std::max(1, static_cast<int>(pool.onlineCount() * CHAT_RATIO)));
            for (auto& c : sample) {
                c->sendChatMsg(uidDist(gen), "stress test message");
            }
        }
    });

    std::atomic<bool> stopMonitoring(false);
    std::thread monitor([&] {
        int t = 0;
        while (!stopMonitoring.load()) {
            std::this_thread::sleep_for(10s);
            if (stopMonitoring.load()) break;
            t += 10;
            report.tick(pool.metrics(), pool.onlineCount(), t);
            auto& m = pool.metrics();
            std::cout << "[t=" << std::setw(4) << t << "s] online=" << pool.onlineCount()
                      << " msg_sent=" << m.msg_sent.load()
                      << " msg_recv=" << m.msg_recv.load()
                      << " rtt_p99=" << m.rtt_hist.percentile(0.99) << "us"
                      << " disc=" << m.disconnect_.load()
                      << std::endl;
        }
    });

    std::cout << "Running sustained load for " << TEST_DURATION_MINUTES << " minutes..." << std::endl;
    std::this_thread::sleep_for(std::chrono::minutes(TEST_DURATION_MINUTES));

    stopProducing.store(true);
    stopMonitoring.store(true);
    producer.join();
    monitor.join();

    auto& m = pool.metrics();
    int finalOnline = pool.onlineCount();

    report.summary(m, TARGET, TEST_DURATION_MINUTES * 60 * 1000);
    report.saveCsv("sustained_load_10k_report.csv");

    std::cout << "\n=== SustainedLoad_10K Results ===" << std::endl;
    std::cout << "Final online:  " << finalOnline << "/" << TARGET << std::endl;
    std::cout << "Survival rate: " << std::fixed << std::setprecision(2)
              << (finalOnline * 100.0 / TARGET) << "%" << std::endl;
    std::cout << "================================\n" << std::endl;

    EXPECT_GE(finalOnline, TARGET * 0.85);
    EXPECT_LT(m.errorRate(), 0.15);

    pool.gracefulShutdown();
}
