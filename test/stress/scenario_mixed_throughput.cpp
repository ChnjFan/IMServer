#include <gtest/gtest.h>

#include "stress_fixture.h"
#include "stress_connection_pool.h"
#include "report_output.h"

#include <chrono>
#include <iostream>
#include <iomanip>
#include <thread>

using namespace std::chrono_literals;

/**
 * @brief 场景 5: 混合消息吞吐量探测
 *
 * 目标: 在 70% 聊天 + 20% 好友申请 + 10% 用户搜索 的混合负载下，
 *       找到服务端消息吞吐的饱和点，约束 RTT P99 < 200ms
 *
 * 策略:
 *   1. 固定连接数
 *   2. 阶梯增加每条连接的发送速率
 *   3. 每档稳定 60s 后采样
 *   4. RTT P99 超过 200ms 或错误率 > 5% 时停止
 */

class MixedThroughputTest : public StressTestFixture {
protected:
    static constexpr int STABILIZE_SECONDS = 60;        // 稳定期 (采样前等待)
    static constexpr int64_t RTT_P99_LIMIT_US = 200000;  // 200ms
    static constexpr double ERROR_THRESHOLD = 0.05;
    static constexpr float CHAT_RATIO = 0.7f;
    static constexpr float FRIEND_RATIO = 0.2f;
    static constexpr float QUERY_RATIO = 0.1f;
};

// 1K 连接混合吞吐探测
TEST_F(MixedThroughputTest, Mixed_1K) {
    constexpr int TARGET = 1000;

    auto accounts = takeAccounts(TARGET);
    ASSERT_GE(static_cast<int>(accounts.size()), TARGET);

    int ioCount = std::max(4, static_cast<int>(std::thread::hardware_concurrency()) - 2);
    StressConnectionPool pool(ioCount);
    ReportOutput report("MixedThroughput_1K");

    pool.addAndConnect(accounts, 100, 200ms);

    // 等待全部上线
    auto deadline = std::chrono::steady_clock::now() + 30s;
    while (pool.onlineCount() < TARGET * 0.95) {
        if (std::chrono::steady_clock::now() > deadline) break;
        std::this_thread::sleep_for(200ms);
    }

    std::cout << "\n=== MixedThroughput_1K: " << pool.onlineCount()
              << " connections online ===" << std::endl;

    int minUid = accounts.front().uid;
    int maxUid = accounts.back().uid;

    // 阶梯速率: 1, 3, 5, 10, 15, 20, 30, 50 msg/s
    int rates[] = {1, 3, 5, 10, 15, 20, 30, 50};

    int breakingRate = -1;
    int64_t breakingP99 = 0;
    double breakingErr = 0;
    int maxSustainableRate = 0;

    std::cout << "\n=== Mixed Throughput Saturation Test (1K connections) ===" << std::endl;
    std::cout << "Rate | TotalMsg/s | P50(us) | P99(us) | ErrRate | Status" << std::endl;
    std::cout << "-----|------------|---------|---------|---------|-------" << std::endl;

    for (int rate : rates) {
        // 设置所有在线客户端的混合发送速率
        auto online = pool.getOnlineClients();
        for (auto& c : online) {
            c->startMixedMsgRate(rate, minUid, maxUid, CHAT_RATIO, FRIEND_RATIO, QUERY_RATIO);
        }

        // 稳定期
        std::this_thread::sleep_for(std::chrono::seconds(STABILIZE_SECONDS));

        // 采样指标
        auto& m = pool.metrics();
        int onlineCount = pool.onlineCount();
        int64_t p50 = m.rtt_hist.percentile(0.5);
        int64_t p99 = m.rtt_hist.percentile(0.99);
        double errRate = m.errorRate();

        // 停止发送
        for (auto& c : online) {
            c->stopMsgRate();
        }

        static int t = 0;
        t += STABILIZE_SECONDS;
        report.tick(m, onlineCount, t);

        std::cout << std::setw(4) << rate << " | "
                  << std::setw(10) << (onlineCount * rate) << " | "
                  << std::setw(7) << p50 << " | "
                  << std::setw(7) << p99 << " | "
                  << std::fixed << std::setprecision(3) << (errRate * 100) << "% | ";

        if (p99 > RTT_P99_LIMIT_US) {
            std::cout << "OVERLOAD (RTT)" << std::endl;
            breakingRate = rate;
            breakingP99 = p99;
            breakingErr = errRate;
            std::cout << "Breaking point at rate=" << rate
                      << " (P99=" << p99 << "us > " << RTT_P99_LIMIT_US << "us)" << std::endl;
            break;
        }

        if (errRate > ERROR_THRESHOLD) {
            std::cout << "OVERLOAD (ERROR)" << std::endl;
            breakingRate = rate;
            breakingP99 = p99;
            breakingErr = errRate;
            std::cout << "Breaking point at rate=" << rate
                      << " (errRate=" << (errRate * 100) << "% > 5%)" << std::endl;
            break;
        }

        std::cout << "OK" << std::endl;
        maxSustainableRate = rate;

        // 重置直方图以获取下一档的准确数据 (放在break之后，保留最后一档数据供summary使用)
        m.rtt_hist.reset();
    }

    std::cout << "================================================\n" << std::endl;

    auto& m = pool.metrics();
    report.summary(m, TARGET, 0);
    report.saveCsv("mixed_throughput_1k_report.csv");

    std::cout << "\n=== MixedThroughput_1K Result ===" << std::endl;
    std::cout << "Max sustainable rate: " << maxSustainableRate << " msg/s per conn" << std::endl;
    std::cout << "Max throughput: " << (maxSustainableRate * TARGET) << " msg/s" << std::endl;
    if (breakingRate > 0) {
        std::cout << "Breaking at: " << breakingRate << " msg/s per conn" << std::endl;
        std::cout << "Breaking P99: " << breakingP99 << " us" << std::endl;
    }
    std::cout << "================================\n" << std::endl;

    // 断言: 在 200ms RTT 限制下至少能达到 5 msg/s/conn (5K msg/s total)
    EXPECT_GE(maxSustainableRate, 5);

    pool.gracefulShutdown();
}

// 5K 连接混合吞吐探测
TEST_F(MixedThroughputTest, Mixed_5K) {
    constexpr int TARGET = 5000;

    auto accounts = takeAccounts(TARGET);
    ASSERT_GE(static_cast<int>(accounts.size()), TARGET);

    int ioCount = std::max(4, static_cast<int>(std::thread::hardware_concurrency()) - 2);
    StressConnectionPool pool(ioCount);
    ReportOutput report("MixedThroughput_5K");

    pool.addAndConnect(accounts, 500, 200ms);

    auto deadline = std::chrono::steady_clock::now() + 60s;
    while (pool.onlineCount() < TARGET * 0.90) {
        if (std::chrono::steady_clock::now() > deadline) break;
        std::this_thread::sleep_for(500ms);
    }

    std::cout << "\n=== MixedThroughput_5K: " << pool.onlineCount()
              << " connections online ===" << std::endl;

    int minUid = accounts.front().uid;
    int maxUid = accounts.back().uid;

    int rates[] = {1, 3, 5, 10, 15, 20, 30};

    int breakingRate = -1;
    int64_t breakingP99 = 0;
    int maxSustainableRate = 0;

    std::cout << "\n=== Mixed Throughput Saturation Test (5K connections) ===" << std::endl;
    std::cout << "Rate | TotalMsg/s | P50(us) | P99(us) | ErrRate | Status" << std::endl;
    std::cout << "-----|------------|---------|---------|---------|-------" << std::endl;

    for (int rate : rates) {
        auto online = pool.getOnlineClients();
        for (auto& c : online) {
            c->startMixedMsgRate(rate, minUid, maxUid, CHAT_RATIO, FRIEND_RATIO, QUERY_RATIO);
        }

        std::this_thread::sleep_for(std::chrono::seconds(STABILIZE_SECONDS));

        auto& m = pool.metrics();
        int onlineCount = pool.onlineCount();
        int64_t p50 = m.rtt_hist.percentile(0.5);
        int64_t p99 = m.rtt_hist.percentile(0.99);
        double errRate = m.errorRate();

        for (auto& c : online) {
            c->stopMsgRate();
        }

        static int t = 0;
        t += STABILIZE_SECONDS;
        report.tick(m, onlineCount, t);

        std::cout << std::setw(4) << rate << " | "
                  << std::setw(10) << (onlineCount * rate) << " | "
                  << std::setw(7) << p50 << " | "
                  << std::setw(7) << p99 << " | "
                  << std::fixed << std::setprecision(3) << (errRate * 100) << "% | ";

        if (p99 > RTT_P99_LIMIT_US) {
            std::cout << "OVERLOAD (RTT)" << std::endl;
            breakingRate = rate;
            breakingP99 = p99;
            std::cout << "Breaking point at rate=" << rate
                      << " (P99=" << p99 << "us > " << RTT_P99_LIMIT_US << "us)" << std::endl;
            break;
        }

        if (errRate > ERROR_THRESHOLD) {
            std::cout << "OVERLOAD (ERROR)" << std::endl;
            breakingRate = rate;
            breakingP99 = p99;
            std::cout << "Breaking point at rate=" << rate
                      << " (errRate=" << (errRate * 100) << "% > 5%)" << std::endl;
            break;
        }

        std::cout << "OK" << std::endl;
        maxSustainableRate = rate;

        // 重置直方图以获取下一档的准确数据 (放在break之后，保留最后一档数据供summary使用)
        m.rtt_hist.reset();
    }

    std::cout << "================================================\n" << std::endl;

    auto& m = pool.metrics();
    report.summary(m, TARGET, 0);
    report.saveCsv("mixed_throughput_5k_report.csv");

    std::cout << "\n=== MixedThroughput_5K Result ===" << std::endl;
    std::cout << "Max sustainable rate: " << maxSustainableRate << " msg/s per conn" << std::endl;
    std::cout << "Max throughput: " << (maxSustainableRate * TARGET) << " msg/s" << std::endl;
    if (breakingRate > 0) {
        std::cout << "Breaking at: " << breakingRate << " msg/s per conn" << std::endl;
        std::cout << "Breaking P99: " << breakingP99 << " us" << std::endl;
    }
    std::cout << "================================\n" << std::endl;

    EXPECT_GE(maxSustainableRate, 3);

    pool.gracefulShutdown();
}

// 10K 连接混合吞吐探测
TEST_F(MixedThroughputTest, Mixed_10K) {
    constexpr int TARGET = 10000;

    auto accounts = takeAccounts(TARGET);
    ASSERT_GE(static_cast<int>(accounts.size()), TARGET);

    int ioCount = std::max(6, static_cast<int>(std::thread::hardware_concurrency()) - 2);
    StressConnectionPool pool(ioCount);
    ReportOutput report("MixedThroughput_10K");

    pool.addAndConnect(accounts, 500, 200ms);

    auto deadline = std::chrono::steady_clock::now() + 120s;
    while (pool.onlineCount() < TARGET * 0.85) {
        if (std::chrono::steady_clock::now() > deadline) break;
        std::this_thread::sleep_for(500ms);
    }

    std::cout << "\n=== MixedThroughput_10K: " << pool.onlineCount()
              << " connections online ===" << std::endl;

    int minUid = accounts.front().uid;
    int maxUid = accounts.back().uid;

    int rates[] = {1, 2, 3, 5, 10, 15, 20};

    int breakingRate = -1;
    int64_t breakingP99 = 0;
    int maxSustainableRate = 0;

    std::cout << "\n=== Mixed Throughput Saturation Test (10K connections) ===" << std::endl;
    std::cout << "Rate | TotalMsg/s | P50(us) | P99(us) | ErrRate | Status" << std::endl;
    std::cout << "-----|------------|---------|---------|---------|-------" << std::endl;

    for (int rate : rates) {
        auto online = pool.getOnlineClients();
        for (auto& c : online) {
            c->startMixedMsgRate(rate, minUid, maxUid, CHAT_RATIO, FRIEND_RATIO, QUERY_RATIO);
        }

        std::this_thread::sleep_for(std::chrono::seconds(STABILIZE_SECONDS));

        auto& m = pool.metrics();
        int onlineCount = pool.onlineCount();
        int64_t p50 = m.rtt_hist.percentile(0.5);
        int64_t p99 = m.rtt_hist.percentile(0.99);
        double errRate = m.errorRate();

        for (auto& c : online) {
            c->stopMsgRate();
        }

        static int t = 0;
        t += STABILIZE_SECONDS;
        report.tick(m, onlineCount, t);

        std::cout << std::setw(4) << rate << " | "
                  << std::setw(10) << (onlineCount * rate) << " | "
                  << std::setw(7) << p50 << " | "
                  << std::setw(7) << p99 << " | "
                  << std::fixed << std::setprecision(3) << (errRate * 100) << "% | ";

        if (p99 > RTT_P99_LIMIT_US) {
            std::cout << "OVERLOAD (RTT)" << std::endl;
            breakingRate = rate;
            breakingP99 = p99;
            std::cout << "Breaking point at rate=" << rate
                      << " (P99=" << p99 << "us > " << RTT_P99_LIMIT_US << "us)" << std::endl;
            break;
        }

        if (errRate > ERROR_THRESHOLD) {
            std::cout << "OVERLOAD (ERROR)" << std::endl;
            breakingRate = rate;
            breakingP99 = p99;
            std::cout << "Breaking point at rate=" << rate
                      << " (errRate=" << (errRate * 100) << "% > 5%)" << std::endl;
            break;
        }

        std::cout << "OK" << std::endl;
        maxSustainableRate = rate;

        // 重置直方图以获取下一档的准确数据 (放在break之后，保留最后一档数据供summary使用)
        m.rtt_hist.reset();
    }

    std::cout << "================================================\n" << std::endl;

    auto& m = pool.metrics();
    report.summary(m, TARGET, 0);
    report.saveCsv("mixed_throughput_10k_report.csv");

    std::cout << "\n=== MixedThroughput_10K Result ===" << std::endl;
    std::cout << "Max sustainable rate: " << maxSustainableRate << " msg/s per conn" << std::endl;
    std::cout << "Max throughput: " << (maxSustainableRate * TARGET) << " msg/s" << std::endl;
    if (breakingRate > 0) {
        std::cout << "Breaking at: " << breakingRate << " msg/s per conn" << std::endl;
        std::cout << "Breaking P99: " << breakingP99 << " us" << std::endl;
    }
    std::cout << "================================\n" << std::endl;

    EXPECT_GE(maxSustainableRate, 1);

    pool.gracefulShutdown();
}
