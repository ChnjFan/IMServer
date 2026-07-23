#ifndef IMSERVER_STRESS_FIXTURE_H
#define IMSERVER_STRESS_FIXTURE_H

#include <gtest/gtest.h>

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#include "account_manager.h"
#include "ConfigMgr.h"
#include "RedisMgr.h"
#include "fixture_base.h"

/**
 * @brief 压力测试夹具
 *
 * 负责:
 *   - 测试前预注册账号 (参考 AccountManager::registerAccount)
 *   - 测试时分发账号给各场景
 *   - 测试后清理账号
 *
 * 账号在 SetUpTestSuite 中一次性注册，缓存在内存中复用。
 */
class StressTestFixture : public IntegrationTestBase {
protected:
    static void SetUpTestSuite() {
        IntegrationTestBase::SetUpTestSuite();
        if (gSkip) return;

        // 注册测试账号 (足够 10K 测试 + 余量)
        s_accounts = registerAccounts_(TARGET_STRESS_ACCOUNTS);
        std::cout << "[StressTestFixture] Registered " << s_accounts.size()
                  << " accounts for stress testing" << std::endl;
    }

    static void TearDownTestSuite() {
        // 清理注册的账号
        AccountManager mgr;
        for (const auto& acct : s_accounts) {
            mgr.release(acct.uid);
        }
        s_accounts.clear();
        s_cursor = 0;
    }

    /** 获取指定数量的账号 */
    static std::vector<TestAccount> takeAccounts(int n) {
        std::lock_guard<std::mutex> lock(s_mutex);
        int available = static_cast<int>(s_accounts.size()) - s_cursor;
        int take = std::min(n, available);
        if (take < n) {
            std::cerr << "[StressTestFixture] Not enough accounts! Requested " << n
                      << " but only " << take << " available" << std::endl;
        }
        auto begin = s_accounts.begin() + s_cursor;
        auto end = begin + take;
        s_cursor += take;
        return std::vector<TestAccount>(begin, end);
    }

    /** 获取 GateServer 地址 */
    static std::string gateHost() { return "127.0.0.1"; }
    static uint16_t gatePort() {
        return static_cast<uint16_t>(
            std::stoi(ConfigMgr::getInstance()["GateServer"]["Port"]));
    }

private:
    static constexpr int TARGET_STRESS_ACCOUNTS = 12000;  // 支持 10K 测试

    static std::vector<TestAccount> registerAccounts_(int count);

    static inline std::vector<TestAccount> s_accounts;
    static inline int s_cursor = 0;
    static inline std::mutex s_mutex;
};

#endif // IMSERVER_STRESS_FIXTURE_H
