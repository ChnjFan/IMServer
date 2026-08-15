#ifndef IMSERVER_ACCOUNT_MANAGER_H
#define IMSERVER_ACCOUNT_MANAGER_H

#include <set>
#include <string>
#include <vector>
#include <memory>
#include "MysqlPool.h"
#include "redis_cleanup.h"

struct TestAccount {
    int uid;
    std::string email;
    std::string token;
    std::string host;
    int port;
};

class AccountManager {
public:
    AccountManager();
    ~AccountManager();

    // Acquire a unique test account (registers via HTTP to GateServer)
    TestAccount acquire(const std::string& tag);

    // Batch acquire for multi-user scenarios
    std::vector<TestAccount> acquireBatch(int n, const std::string& tag);

    void login(TestAccount& acct);

    // Release a specific account (never throws)
    void release(int uid) noexcept;

    // Release all acquired accounts (never throws)
    void releaseAll() noexcept;

private:
    inline static std::atomic<int> userCount{0};

    std::set<int> heldUids_;
    std::map<int, std::string> uidToEmail_;  // track emails for Redis cleanup
    std::string gateHost_;
    uint16_t gatePort_;
    std::unique_ptr<MysqlPool> mysqlPool_;

    // Fetch verify code from Redis (set by GateServer after /get_verify_code)
    std::string fetchVerifyCode(const std::string& email);

    // Register via GateServer HTTP API
    TestAccount registerAccount(const std::string& email);
};


#endif
