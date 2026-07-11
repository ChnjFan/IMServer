#ifndef IMSERVER_FIXTURE_BASE_H
#define IMSERVER_FIXTURE_BASE_H

#include <gtest/gtest.h>
#include "account_manager.h"
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
        if (accountMgr_) {
            accountMgr_->releaseAll();
        }
    }

    static inline bool gSkip = false;
    std::unique_ptr<AccountManager> accountMgr_;
};

#endif
