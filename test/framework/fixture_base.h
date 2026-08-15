#ifndef IMSERVER_FIXTURE_BASE_H
#define IMSERVER_FIXTURE_BASE_H

#include <gtest/gtest.h>
#include <iostream>
#include "account_manager.h"
#include "redis_cleanup.h"
#include "ConfigMgr.h"
#include "RedisMgr.h"

class IntegrationTestBase : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        gSkip = false;
        try {
            // Test Redis reachability
            auto redis = RedisMgr::getInstance();
            if (!redis->set("__test_ping__", "1")) {
                gSkip = true;
                std::cerr << "[IntegrationTestBase] Redis set failed, skipping"
                          << std::endl;
                return;
            }
            std::string val;
            if (!redis->get("__test_ping__", val) || val != "1") {
                gSkip = true;
                std::cerr << "[IntegrationTestBase] Redis unreachable, skipping"
                          << std::endl;
            }
            redis->del("__test_ping__");  // clean up the probe key
        } catch (const std::exception& e) {
            gSkip = true;
            std::cerr << "[IntegrationTestBase] Dependency check failed: "
                      << e.what() << std::endl;
        }
    }

    void SetUp() override {
        if (gSkip) {
            GTEST_SKIP() << "External dependencies unavailable";
        }
        accountMgr_ = std::make_unique<AccountManager>();
    }

    void TearDown() override {
        try {
            if (accountMgr_) {
                accountMgr_->releaseAll();
            }
        } catch (const std::exception& e) {
            // 清理失败不抛，避免 cascade；输出日志供排查
            std::cerr << "[IntegrationTestBase] TearDown cleanup error: "
                      << e.what() << std::endl;
        } catch (...) {
            std::cerr << "[IntegrationTestBase] TearDown unknown cleanup error"
                      << std::endl;
        }
        // 最后兜底：即使 accountMgr_ 析构前异常，也扫描残留 test key
        cleanupOrphanedTestKeys();
    }

    static inline bool gSkip = false;
    std::unique_ptr<AccountManager> accountMgr_;
};

#endif
