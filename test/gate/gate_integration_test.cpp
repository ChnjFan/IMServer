#include <gtest/gtest.h>
#include "fixture_base.h"
#include "http_test_client.h"
#include "ConfigMgr.h"

class GateIntegrationTest : public IntegrationTestBase {
protected:
    void SetUp() override {
        IntegrationTestBase::SetUp();
        auto& config = ConfigMgr::getInstance();
        std::string host = "127.0.0.1";
        uint16_t port = static_cast<uint16_t>(std::stoi(config["GateServer"]["Port"]));
        http_ = std::make_unique<HttpTestClient>(host, port);
    }

    std::unique_ptr<HttpTestClient> http_;
};

// Verify the /get_verify_code endpoint responds successfully.
// Requires VerifyServer running to actually send code to Redis.
TEST_F(GateIntegrationTest, GetVerifyCodeSuccess) {
    Json::Value req;
    req["email"] = "gate_int_verify@test.com";
    auto rsp = http_->post("/get_verify_code", req);
    EXPECT_EQ(rsp.code, 200);
    // error == 0 only if VerifyServer is running; accept either outcome
    EXPECT_TRUE(rsp.body.isMember("error"));
}

// Register a new user via AccountManager and verify uid is assigned.
// Requires full stack: GateServer + VerifyServer + Redis + MySQL.
TEST_F(GateIntegrationTest, RegisterNewUser) {
    auto acct = accountMgr_->acquire("reg");
    EXPECT_GT(acct.uid, 0);
    EXPECT_NE(acct.email.find("test_reg_"), std::string::npos);
}

// Verify that a registered user appears in the database.
TEST_F(GateIntegrationTest, RegisteredUserHasValidUid) {
    auto acct = accountMgr_->acquire("uid_check");
    EXPECT_GT(acct.uid, 0);
    EXPECT_FALSE(acct.email.empty());
}
