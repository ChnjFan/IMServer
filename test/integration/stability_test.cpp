#include <gtest/gtest.h>
#include "fixture_base.h"
#include "stability_runner.h"
#include "ConfigMgr.h"

class StabilityTest : public IntegrationTestBase {};

// 心跳长跑 30 分钟（完整 24h 用 durationSec = 86400）
TEST_F(StabilityTest, KeepAlive30Min) {
    auto& config = ConfigMgr::getInstance();
    StabilityConfig cfg;
    cfg.chatHost = "127.0.0.1";
    cfg.chatPort = static_cast<uint16_t>(std::stoi(config["ChatServer"]["Port"]));
    cfg.clientCount = 5;
    cfg.durationSec = 1800; // 30 分钟

    StabilityRunner runner(cfg);
    EXPECT_TRUE(runner.runKeepAlive());
}

// 混合场景 1 小时（心跳 + 上下线 + 消息风暴）
TEST_F(StabilityTest, Mixed1Hour) {
    auto& config = ConfigMgr::getInstance();
    StabilityConfig cfg;
    cfg.chatHost = "127.0.0.1";
    cfg.chatPort = static_cast<uint16_t>(std::stoi(config["ChatServer"]["Port"]));
    cfg.clientCount = 10;
    cfg.durationSec = 3600;

    StabilityRunner runner(cfg);
    EXPECT_TRUE(runner.runMixed());
    runner.report().writeDataFile(cfg.reportPath);
}
