#include "account_manager.h"
#include "http_test_client.h"
#include "ConfigMgr.h"
#include "RedisMgr.h"
#include "MysqlPool.h"
#include "mysql_dao.h"

#include <iostream>
#include <memory>

// RAII guard：析构时自动把连接归还池，即使发生异常也不会泄露
struct SqlConnGuard {
    MysqlPool* pool;
    std::unique_ptr<SqlConnection> conn;
    SqlConnGuard(MysqlPool* p) : pool(p), conn(p ? p->getConnect() : nullptr) {}
    ~SqlConnGuard() { if (pool && conn) pool->returnConnect(std::move(conn)); }
    SqlConnection* operator->() const { return conn.get(); }
    explicit operator bool() const { return conn && conn->conn_; }
};

AccountManager::AccountManager() {
    auto& config = ConfigMgr::getInstance();
    gateHost_ = "127.0.0.1";
    gatePort_ = static_cast<uint16_t>(std::stoi(config["GateServer"]["Port"]));
}

AccountManager::~AccountManager() {
    releaseAll();
}

std::string AccountManager::fetchVerifyCode(const std::string& email) {
    auto redis = RedisMgr::getInstance();
    std::string key = CODE_PREFIX + email;
    std::string value = "123456"; // Default code
    if (redis->set(key, value)) {
        return value;
    }
    return {};
}

TestAccount AccountManager::registerAccount(const std::string& email) {
    TestAccount acct;
    acct.email = email;

    HttpTestClient http(gateHost_, gatePort_);

    // 要写入一个验证码到 redis 中
    std::string code = fetchVerifyCode(email);

    std::string username = email.substr(0, email.find('@'));
    Json::Value regReq;
    regReq["email"] = email;
    regReq["user"] = username;
    regReq["password"] = "Test123456";
    regReq["confirm"] = "Test123456";
    regReq["verify_code"] = code;
    auto regRsp = http.post("/user_register", regReq);

    acct.uid = regRsp.body["uid"].asInt();
    return acct;
}

TestAccount AccountManager::acquire(const std::string& tag) {
    std::string email = "test_" + tag + "_" + std::to_string(++userCount) + "@test.com";
    TestAccount acct = registerAccount(email);
    heldUids_.insert(acct.uid);
    uidToEmail_[acct.uid] = acct.email;
    recordTestEmail(acct.email);  // 记录到 set，供兜底清理
    return acct;
}

std::vector<TestAccount> AccountManager::acquireBatch(int n, const std::string& tag) {
    std::vector<TestAccount> accounts;
    for (int i = 0; i < n; i++) {
        accounts.push_back(acquire(tag + "_" + std::to_string(i)));
    }
    return accounts;
}

void AccountManager::login(TestAccount &acct) {
    HttpTestClient http(gateHost_, gatePort_);

    Json::Value loginReq;
    loginReq["email"] = acct.email;
    loginReq["password"] = "Test123456";
    auto loginRsp = http.post("/user_login", loginReq);
    if (loginRsp.body["port"].asString().empty()) {
        return; // 登录失败，可能是 GateServer 未启动
    }
    acct.token = loginRsp.body["token"].asString();
    acct.host = loginRsp.body["host"].asString();
    acct.port = std::stoi(loginRsp.body["port"].asString());
}

void AccountManager::release(int uid) noexcept {
    heldUids_.erase(uid);

    try {
        // 删除 fetchVerifyCode 写入 Redis 的验证码
        if (auto it = uidToEmail_.find(uid); it != uidToEmail_.end()) {
            try {
                auto redis = RedisMgr::getInstance();
                redis->del(CODE_PREFIX + it->second);
            } catch (const std::exception& e) {
                std::cerr << "[AccountManager] Redis del failed for uid="
                          << uid << ": " << e.what() << std::endl;
            }
            uidToEmail_.erase(it);
        }

        // 通过 MySQL 删除用户记录（RAII guard 保证连接必归还）
        SqlConnGuard guard(TestMysqlDao::getInstance()->get());
        if (guard) {
            std::unique_ptr<sql::Statement> stmt(guard->conn_->createStatement());
            stmt->execute("DELETE FROM user WHERE uid = " +
                         std::to_string(uid));
        }
    } catch (sql::SQLException& e) {
        std::cerr << "[AccountManager] release uid=" << uid
                  << " SQL error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[AccountManager] release uid=" << uid
                  << " error: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "[AccountManager] release uid=" << uid
                  << " unknown error" << std::endl;
    }
}

void AccountManager::releaseAll() noexcept {
    // release() erases from heldUids_, so iterate a copy to avoid
    // iterator invalidation.
    auto uids = heldUids_;
    for (const auto uid : uids) {
        release(uid);
    }
    heldUids_.clear();
    // 兜底：清理异常路径中 heldUids_ 遗漏的残留 key（幂等）
    cleanupOrphanedTestKeys();
}
