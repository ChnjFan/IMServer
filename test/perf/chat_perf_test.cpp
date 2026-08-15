#include <gtest/gtest.h>
#include "fixture_base.h"
#include "perf_suite.h"
#include "ConfigMgr.h"

class PerfTest : public IntegrationTestBase {};

// 阶梯施压：记录每个并发台阶的 QPS / P50 / P99
TEST_F(PerfTest, RampUp) {
    auto& config = ConfigMgr::getInstance();
    PerfSuite::Config cfg;
    cfg.stepSec = 60;

    PerfSuite suite(cfg);
    auto results = suite.runRampUp();

    // 延迟应在合理范围（P99 < 10ms）
    for (auto& r : results) {
        EXPECT_LT(r.p99_us, 10'000) << "client=" << r.clientCount;
        std::cout << "[perf] clients=" << r.clientCount
                  << " qps=" << r.qps
                  << " p50=" << r.p50_us << "us"
                  << " p99=" << r.p99_us << "us"
                  << " err=" << r.errorRate * 100 << "%\n";
    }
}

// 极限施压：找到崩溃点（错误率 > 5% 的并发数）
TEST_F(PerfTest, ToBreak) {
    auto& config = ConfigMgr::getInstance();
    PerfSuite::Config cfg;
    cfg.stepSec = 30;

    PerfSuite suite(cfg);
    int crashPoint = suite.runToBreak();

    // 崩溃点应存在（即 2000 并发以内会出现崩溃）
    EXPECT_GT(crashPoint, 0);
    std::cout << "[perf] crash point: " << crashPoint << " clients\n";
}

// 混合负载读写场景
TEST_F(PerfTest, MixedWorkload) {
    auto& config = ConfigMgr::getInstance();
    PerfSuite::Config cfg;
    cfg.stepSec = 60;

    PerfSuite suite(cfg);
    auto lvl = suite.runMixedWorkload(100);

    EXPECT_LT(lvl.errorRate, 0.05);
    EXPECT_GT(lvl.qps, 0);
}
