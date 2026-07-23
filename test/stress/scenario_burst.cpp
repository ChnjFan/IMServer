#include <gtest/gtest.h>

#include "stress_fixture.h"
#include "stress_connection_pool.h"
#include "report_output.h"

#include <chrono>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

/**
 * @brief 场景 1: 瞬时连接风暴
 *
 * 测试目标: 验证服务器在短时间内大量连接建立时的处理能力
 */

class BurstConnectTest : public StressTestFixture {
protected:
    static constexpr int IO_CONTEXT_NUM = 8;
};

// 100 连接快速验证 (开发调试)
TEST_F(BurstConnectTest, Connect_100_Fast) {
    constexpr int TARGET = 100;

    auto accounts = takeAccounts(TARGET);
    ASSERT_GE(static_cast<int>(accounts.size()), TARGET);

    // 打印前 3 个账号信息用于调试
    std::cout << "\n=== Account Info Debug ===" << std::endl;
    for (int i = 0; i < std::min(3, (int)accounts.size()); i++) {
        std::cout << "Account[" << i << "]: uid=" << accounts[i].uid
                  << " host=" << accounts[i].host
                  << " port=" << accounts[i].port << std::endl;
    }
    std::cout << "==========================\n" << std::endl;

    int ioCount = std::max(4, static_cast<int>(std::thread::hardware_concurrency()) - 2);
    StressConnectionPool pool(ioCount);
    ReportOutput report("BurstConnect_100");

    pool.addAndConnect(accounts, 50, 100ms);

    int elapsed = 0;
    auto deadline = std::chrono::steady_clock::now() + 10s;
    while (pool.onlineCount() < TARGET) {
        if (std::chrono::steady_clock::now() > deadline) break;
        std::this_thread::sleep_for(1s);
        elapsed++;
        report.tick(pool.metrics(), pool.onlineCount(), elapsed);
    }

    auto& m = pool.metrics();
    report.summary(m, TARGET, elapsed * 1000);
    report.saveCsv("burst_connect_100_report.csv");

    std::cout << "\n=== Connect_100_Fast Results ===" << std::endl;
    std::cout << "Online: " << pool.onlineCount() << "/" << TARGET << std::endl;
    std::cout << "Connect success: " << m.connect_success.load() << std::endl;
    std::cout << "Connect failed: " << m.connect_failed.load() << std::endl;
    std::cout << "Connect timeout: " << m.connect_timeout.load() << std::endl;
    std::cout << "Handshake success: " << m.handshake_success.load() << std::endl;
    std::cout << "Handshake failed: " << m.handshake_failed.load() << std::endl;
    std::cout << "================================\n" << std::endl;

    EXPECT_GE(pool.onlineCount(), 95);
    pool.gracefulShutdown();
}

// 1K 连接风暴 (标准测试)
TEST_F(BurstConnectTest, Connect_1K) {
    constexpr int TARGET = 1000;

    auto accounts = takeAccounts(TARGET);
    ASSERT_GE(static_cast<int>(accounts.size()), TARGET);

    int ioCount = std::max(4, static_cast<int>(std::thread::hardware_concurrency()) - 2);
    StressConnectionPool pool(ioCount);
    ReportOutput report("BurstConnect_1K");

    auto startTime = std::chrono::steady_clock::now();

    // 200个/批, 200ms间隔 → 1000 conn/s
    pool.addAndConnect(accounts, 200, 200ms);

    int elapsed = 0;
    auto deadline = std::chrono::steady_clock::now() + 30s;
    while (pool.onlineCount() < TARGET) {
        if (std::chrono::steady_clock::now() > deadline) break;
        std::this_thread::sleep_for(1s);
        elapsed++;
        report.tick(pool.metrics(), pool.onlineCount(), elapsed);
    }

    auto totalElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime).count();

    auto& m = pool.metrics();
    report.summary(m, TARGET, static_cast<int>(totalElapsed));
    report.saveCsv("burst_connect_1k_report.csv");

    EXPECT_GE(m.connect_success.load(), TARGET * 0.99);
    EXPECT_GE(m.handshake_success.load(), TARGET * 0.98);

    pool.gracefulShutdown();
}

// 5K 连接风暴 (高压力测试)
TEST_F(BurstConnectTest, Connect_5K) {
    constexpr int TARGET = 5000;

    auto accounts = takeAccounts(TARGET);
    ASSERT_GE(static_cast<int>(accounts.size()), TARGET);

    int ioCount = std::max(4, static_cast<int>(std::thread::hardware_concurrency()) - 2);
    StressConnectionPool pool(ioCount);
    ReportOutput report("BurstConnect_5K");

    auto startTime = std::chrono::steady_clock::now();

    // 500个/批, 200ms间隔 → 2500 conn/s
    pool.addAndConnect(accounts, 500, 200ms);

    int elapsed = 0;
    auto deadline = std::chrono::steady_clock::now() + 60s;
    while (pool.onlineCount() < TARGET * 0.95) {
        if (std::chrono::steady_clock::now() > deadline) break;
        std::this_thread::sleep_for(1s);
        elapsed++;
        report.tick(pool.metrics(), pool.onlineCount(), elapsed);
    }

    auto totalElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime).count();

    auto& m = pool.metrics();
    report.summary(m, TARGET, static_cast<int>(totalElapsed));
    report.saveCsv("burst_connect_5k_report.csv");

    std::cout << "\n=== BurstConnect_5K Results ===" << std::endl;
    std::cout << "Online: " << pool.onlineCount() << "/" << TARGET << std::endl;
    std::cout << "Connect success: " << m.connect_success.load() << std::endl;
    std::cout << "Handshake success: " << m.handshake_success.load() << std::endl;
    std::cout << "Elapsed: " << totalElapsed << " ms" << std::endl;
    std::cout << "===============================\n" << std::endl;

    EXPECT_GE(pool.onlineCount(), TARGET * 0.90);
    EXPECT_LT(m.errorRate(), 0.10);

    pool.gracefulShutdown();
}

// 10K 连接风暴 (极限压力测试 — Mac mini M4 单机上限)
TEST_F(BurstConnectTest, Connect_10K) {
    constexpr int TARGET = 10000;

    auto accounts = takeAccounts(TARGET);
    ASSERT_GE(static_cast<int>(accounts.size()), TARGET);

    int ioCount = std::max(4, static_cast<int>(std::thread::hardware_concurrency()) - 2);
    StressConnectionPool pool(ioCount);
    ReportOutput report("BurstConnect_10K");

    auto startTime = std::chrono::steady_clock::now();

    // 500个/批, 200ms间隔 → 2500 conn/s, 全部连接约需 4s
    pool.addAndConnect(accounts, 500, 200ms);

    int elapsed = 0;
    auto deadline = std::chrono::steady_clock::now() + 120s;
    while (pool.onlineCount() < TARGET * 0.90) {
        if (std::chrono::steady_clock::now() > deadline) break;
        std::this_thread::sleep_for(1s);
        elapsed++;
        report.tick(pool.metrics(), pool.onlineCount(), elapsed);
    }

    auto totalElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime).count();

    auto& m = pool.metrics();
    report.summary(m, TARGET, static_cast<int>(totalElapsed));
    report.saveCsv("burst_connect_10k_report.csv");

    std::cout << "\n=== BurstConnect_10K Results ===" << std::endl;
    std::cout << "Online: " << pool.onlineCount() << "/" << TARGET << std::endl;
    std::cout << "Connect success: " << m.connect_success.load() << std::endl;
    std::cout << "Handshake success: " << m.handshake_success.load() << std::endl;
    std::cout << "Elapsed: " << totalElapsed << " ms" << std::endl;
    std::cout << "RTT P50: " << m.rtt_hist.percentile(0.5) << " us" << std::endl;
    std::cout << "RTT P99: " << m.rtt_hist.percentile(0.99) << " us" << std::endl;
    std::cout << "================================\n" << std::endl;

    EXPECT_GE(pool.onlineCount(), TARGET * 0.85);
    EXPECT_LT(m.errorRate(), 0.15);

    pool.gracefulShutdown();
}
