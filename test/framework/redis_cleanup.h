#ifndef IMSERVER_REDIS_CLEANUP_H
#define IMSERVER_REDIS_CLEANUP_H

#include "RedisMgr.h"
#include "const.h"

// Redis set 的 key：记录所有测试申请过的 email
// 用于异常导致 heldUids_ 遗漏时的兜底清理
inline constexpr const char* TEST_EMAIL_SET_KEY = CODE_PREFIX "test_emails";

// 兜底清理：读 set 逐一删除验证码，最后删 set 自身
// 幂等的——重复调用无害；异常时吞掉不抛，不影响 TearDown
inline void cleanupOrphanedTestKeys() noexcept {
    try {
        auto redis = RedisMgr::getInstance();
        auto emails = redis->sMembers(TEST_EMAIL_SET_KEY);
        for (const auto& email : emails) {
            redis->del(CODE_PREFIX + email);
        }
        redis->del(TEST_EMAIL_SET_KEY);
    } catch (...) {
        // 兜底本身失败不抛，避免 cascade
    }
}

// 生成测试账号时记录 email 到 set，供兜底清理用
inline void recordTestEmail(const std::string& email) noexcept {
    try {
        RedisMgr::getInstance()->sAdd(TEST_EMAIL_SET_KEY, email);
        RedisMgr::getInstance()->set(CODE_PREFIX + email, "123456");
    } catch (...) {
        // 记录失败不阻断注册流程
    }
}

#endif
