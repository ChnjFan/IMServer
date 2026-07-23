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
 * @brief 场景 2: 阶梯加压 — 找到服务器吞吐拐点
 *
 * 测试梯度: 500 → 10000，每档 +500
 * 稳定时间: 3s (快速测试)
 * 终止条件: 错误率 > 10% 或在线率 < 80%
 */

class RampUpTest : public StressTestFixture {
protected:
    static constexpr int IO_CONTEXT_NUM = 8;       // 根据 CPU 核心数调整
    static constexpr int STEP_SIZE = 500;          // 每档增加 500
    static constexpr int STABILIZE_SECONDS = 3;    // 稳定 3s
    static constexpr double ERROR_THRESHOLD = 0.10; // 错误率阈值 10%
    static constexpr double ONLINE_THRESHOLD = 0.80; // 在线率阈值 80%
    static constexpr int MAX_STEP = 20;            // 最多 20 档 (500→10000)
};

TEST_F(RampUpTest, FindBreakingPoint) {
    // 动态计算 io_context 数量
    int ioCount = std::max(4, static_cast<int>(std::thread::hardware_concurrency()) - 2);
    std::cout << "[RampUp] Using " << ioCount << " io_contexts" << std::endl;

    StressConnectionPool pool(ioCount);
    ReportOutput report("RampUp_10K");

    int currentTarget = 0;
    int breakingPoint = -1;
    bool stoppedByAccounts = false;

    std::cout << "\n=== RampUp: Finding Breaking Point ===" << std::endl;
    std::cout << "Step | Target | Online | Rate  | P50(us) | P99(us) | ErrRate | Status" << std::endl;
    std::cout << "-----|--------|--------|-------|---------|---------|---------|-------" << std::endl;

    for (int step = 1; step <= MAX_STEP; step++) {
        currentTarget = step * STEP_SIZE;

        // 取新账号并连接
        auto accounts = takeAccounts(STEP_SIZE);
        if (accounts.empty()) {
            std::cout << "No more accounts available, stopping at target=" << currentTarget << std::endl;
            stoppedByAccounts = true;
            break;
        }

        auto startTime = std::chrono::steady_clock::now();

        // 分批连接: 100个/批, 200ms间隔 → 500 conn/s
        pool.addAndConnect(accounts, 100, 200ms);

        // 等待连接建立
        std::this_thread::sleep_for(1s);

        // 稳定期
        std::this_thread::sleep_for(std::chrono::seconds(STABILIZE_SECONDS));

        // 采样指标
        auto& m = pool.metrics();
        int online = pool.onlineCount();
        double errRate = m.errorRate();
        double onlineRate = static_cast<double>(online) / currentTarget;
        int64_t p50 = m.rtt_hist.percentile(0.5);
        int64_t p99 = m.rtt_hist.percentile(0.99);

        // 采样到报告
        report.tick(m, online, step * (1 + STABILIZE_SECONDS));

        std::cout << std::setw(4) << step << " | "
                  << std::setw(6) << currentTarget << " | "
                  << std::setw(6) << online << " | "
                  << std::fixed << std::setprecision(1) << (onlineRate * 100) << "% | "
                  << std::setw(7) << p50 << " | "
                  << std::setw(7) << p99 << " | "
                  << std::setprecision(2) << (errRate * 100) << "% | ";

        if (errRate > ERROR_THRESHOLD) {
            std::cout << "OVERLOAD" << std::endl;
            breakingPoint = currentTarget;
            std::cout << "Breaking point found at " << currentTarget
                      << " (error rate " << (errRate * 100) << "%)" << std::endl;
            break;
        }

        if (onlineRate < ONLINE_THRESHOLD) {
            std::cout << "UNSTABLE" << std::endl;
            breakingPoint = currentTarget;
            std::cout << "Unstable at " << currentTarget
                      << " (online rate " << (onlineRate * 100) << "%)" << std::endl;
            break;
        }

        std::cout << "OK" << std::endl;
    }

    std::cout << "===========================================\n" << std::endl;

    // 如果是因为账号用完而停止，说明服务器未崩溃
    if (stoppedByAccounts && breakingPoint < 0) {
        breakingPoint = currentTarget;
        std::cout << "Server handled all " << currentTarget << " connections without breaking" << std::endl;
    }

    // 输出最终报告
    report.summary(pool.metrics(), currentTarget, 0);
    report.saveCsv("ramp_up_report.csv");

    // 断言: 至少能承受 1000 连接
    EXPECT_GE(breakingPoint, 1000);

    pool.gracefulShutdown();
}
