#ifndef IMSERVER_ACCOUNT_MANAGER_H
#define IMSERVER_ACCOUNT_MANAGER_H

#include <set>
#include <string>
#include <vector>
#include <memory>
#include "MysqlPool.h"

struct TestAccount {
    int uid;
    std::string email;
    std::string token;
};

class AccountManager {
public:
    AccountManager();
    ~AccountManager();

    // Acquire a unique test account (registers via HTTP to GateServer)
    TestAccount acquire(const std::string& tag);

    // Batch acquire for multi-user scenarios
    std::vector<TestAccount> acquireBatch(int n, const std::string& tag);

    // Release a specific account
    void release(int uid);

    // Release all acquired accounts
    void releaseAll();

private:
    std::set<int> heldUids_;
    int seq_ = 0;
    std::string gateHost_;
    uint16_t gatePort_;
    std::unique_ptr<MysqlPool> mysqlPool_;

    // Fetch verify code from Redis (set by GateServer after /get_verify_code)
    std::string fetchVerifyCode(const std::string& email);

    // Register via GateServer HTTP API
    TestAccount registerAccount(const std::string& email);
};

#endif
