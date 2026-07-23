#include "stress_fixture.h"
#include "http_test_client.h"
#include "redis_cleanup.h"

#include <iostream>
#include <sstream>
#include <thread>
#include <vector>
#include <atomic>

std::vector<TestAccount> StressTestFixture::registerAccounts_(int count) {
    std::vector<TestAccount> accounts;
    accounts.reserve(count);

    auto& config = ConfigMgr::getInstance();
    std::string gateHost = "127.0.0.1";
    uint16_t gatePort = static_cast<uint16_t>(std::stoi(config["GateServer"]["Port"]));

    std::atomic<int> registered{0};
    std::atomic<int> failed{0};

    for (int i = 0; i < count; i++) {
        std::string email = "stress_" + std::to_string(i) + "@test.com";
        std::string username = "stress_user_" + std::to_string(i);

        try {
            // 写验证码到 Redis
            recordTestEmail(email);

            // HTTP 注册
            HttpTestClient http(gateHost, gatePort);

            Json::Value regReq;
            regReq["email"] = email;
            regReq["user"] = username;
            regReq["password"] = "Test123456";
            regReq["confirm"] = "Test123456";
            regReq["verify_code"] = "123456";

            auto regRsp = http.post("/user_register", regReq);
            if (regRsp.code != 200 || !regRsp.body.isMember("uid")) {
                failed++;
                std::cerr << "[StressTestFixture] Register failed for " << email
                          << ": code=" << regRsp.code << std::endl;
                continue;
            }

            TestAccount acct;
            acct.uid = regRsp.body["uid"].asInt();
            acct.email = email;

            // HTTP 登录获取 token
            Json::Value loginReq;
            loginReq["email"] = email;
            loginReq["password"] = "Test123456";
            auto loginRsp = http.post("/user_login", loginReq);

            if (loginRsp.code != 200 || !loginRsp.body.isMember("token")) {
                failed++;
                std::cerr << "[StressTestFixture] Login failed for " << email << std::endl;
                continue;
            }

            acct.token = loginRsp.body["token"].asString();
            acct.host = loginRsp.body["host"].asString();
            acct.port = std::stoi(loginRsp.body["port"].asString());

            accounts.push_back(acct);
            registered++;

            // 每 500 个输出进度
            if (registered % 500 == 0) {
                std::cout << "[StressTestFixture] Registered " << registered
                          << "/" << count << std::endl;
            }

        } catch (const std::exception& e) {
            failed++;
            std::cerr << "[StressTestFixture] Exception for " << email
                      << ": " << e.what() << std::endl;
        }
    }

    std::cout << "[StressTestFixture] Registration complete: "
              << registered << " success, " << failed << " failed" << std::endl;

    // 打印前 3 个账号信息用于调试
    if (!accounts.empty()) {
        std::cout << "\n=== Registered Account Sample ===" << std::endl;
        for (int i = 0; i < std::min(3, (int)accounts.size()); i++) {
            std::cout << "Account[" << i << "]: uid=" << accounts[i].uid
                      << " host=" << accounts[i].host
                      << " port=" << accounts[i].port << std::endl;
        }
        std::cout << "=================================\n" << std::endl;
    }

    return accounts;
}
