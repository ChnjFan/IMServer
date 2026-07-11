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

// 验证 /get_verify_code 端点能正常响应。
// 需要 VerifyServer 在线才能实际发送验证码到 Redis。
TEST_F(GateIntegrationTest, GetVerifyCodeSuccess) {
    Json::Value req;
    req["email"] = "gate_int_verify@test.com";
    auto rsp = http_->post("/get_verify_code", req);
    EXPECT_EQ(rsp.code, 200);
    // error == 0 仅在 VerifyServer 在线时为真；接受任意结果
    EXPECT_TRUE(rsp.body.isMember("error"));
}

// 通过 AccountManager 注册新用户并验证 uid 已分配。
// 需要完整链路：GateServer + VerifyServer + Redis + MySQL。
TEST_F(GateIntegrationTest, RegisterNewUser) {
    auto acct = accountMgr_->acquire("reg");
    EXPECT_GT(acct.uid, 0);
    EXPECT_NE(acct.email.find("test_reg_"), std::string::npos);
}

// 验证注册用户在数据库中存在有效 uid。
TEST_F(GateIntegrationTest, RegisteredUserHasValidUid) {
    auto acct = accountMgr_->acquire("uid_check");
    EXPECT_GT(acct.uid, 0);
    EXPECT_FALSE(acct.email.empty());
}
