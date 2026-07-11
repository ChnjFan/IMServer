#include "account_manager.h"
#include "http_test_client.h"
#include "ConfigMgr.h"
#include "RedisMgr.h"
#include "MysqlPool.h"

#include <iostream>
#include <memory>

AccountManager::AccountManager() {
    auto& config = ConfigMgr::getInstance();
    gateHost_ = "127.0.0.1";
    gatePort_ = static_cast<uint16_t>(
        std::stoi(config["GateServer"]["Port"]));

    // Own a MySQL connection pool for test data cleanup
    std::string host = config["Mysql"]["Host"];
    std::string port = config["Mysql"]["Port"];
    std::string user = config["Mysql"]["User"];
    std::string pwd = config["Mysql"]["Password"];
    std::string schema = config["Mysql"]["Schema"];
    mysqlPool_ = std::make_unique<MysqlPool>(host + ":" + port, user, pwd, schema, 2);
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

    // 1. Request verify code (GateServer sends it via VerifyServer -> Redis)
    Json::Value verifyReq;
    verifyReq["email"] = email;
    http.post("/get_verify_code", verifyReq);

    // 要写入一个验证码到 redis 中
    std::string code = fetchVerifyCode(email);

    // 3. Register with the real API fields: user, password, confirm, verify_code
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
    std::string email = "test_" + tag + "_" + std::to_string(++seq_) + "@test.com";
    TestAccount acct = registerAccount(email);
    heldUids_.insert(acct.uid);
    uidToEmail_[acct.uid] = acct.email;
    return acct;
}

std::vector<TestAccount> AccountManager::acquireBatch(int n, const std::string& tag) {
    std::vector<TestAccount> accounts;
    for (int i = 0; i < n; i++) {
        accounts.push_back(acquire(tag + "_" + std::to_string(i)));
    }
    return accounts;
}

void AccountManager::release(int uid) {
    heldUids_.erase(uid);

    // Delete the verify code that fetchVerifyCode wrote to Redis
    if (auto it = uidToEmail_.find(uid); it != uidToEmail_.end()) {
        auto redis = RedisMgr::getInstance();
        redis->del(CODE_PREFIX + it->second);
        uidToEmail_.erase(it);
    }

    // Delete user from DB directly via MySQL for cleanup
    auto sqlCon = mysqlPool_->getConnect();
    if (sqlCon && sqlCon->conn_) {
        try {
            std::unique_ptr<sql::Statement> stmt(sqlCon->conn_->createStatement());
            stmt->execute("DELETE FROM user WHERE uid = " +
                         std::to_string(uid));
        } catch (sql::SQLException& e) {
            std::cerr << "AccountManager release error: " << e.what() << std::endl;
        }
    }
}

void AccountManager::releaseAll() {
    for (int uid : heldUids_) {
        release(uid);
    }
    heldUids_.clear();
}
