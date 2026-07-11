#include <gtest/gtest.h>
#include "fixture_base.h"
#include "grpc_test_client.h"
#include "message.grpc.pb.h"
#include "ConfigMgr.h"

using message::GetChatServerReq;
using message::GetChatServerRsp;
using message::LoginReq;
using message::LoginRsp;
using message::VerifyTokenReq;
using message::VerifyTokenRsp;
using message::StatusService;

class StatusIntegrationTest : public IntegrationTestBase {
protected:
    void SetUp() override {
        IntegrationTestBase::SetUp();
        auto& config = ConfigMgr::getInstance();
        std::string host = "127.0.0.1";
        uint16_t port = static_cast<uint16_t>(std::stoi(config["StatusServer"]["Port"]));
        grpc_ = std::make_unique<GrpcTestClient<StatusService>>(host, port);
    }

    std::unique_ptr<GrpcTestClient<StatusService>> grpc_;
};

// Verify GetChatServer returns a valid host, port, and token.
TEST_F(StatusIntegrationTest, GetChatServerReturnsHostAndToken) {
    GetChatServerReq req;
    req.set_uid(4001);
    GetChatServerRsp rsp;
    grpc::ClientContext ctx;

    auto status = grpc_->stub()->GetChatServer(&ctx, req, &rsp);
    EXPECT_TRUE(status.ok());
    EXPECT_FALSE(rsp.host().empty());
    EXPECT_FALSE(rsp.token().empty());
    EXPECT_EQ(rsp.error(), 0);
}

// Verify Login with a valid token succeeds.
TEST_F(StatusIntegrationTest, LoginWithValidToken) {
    // First obtain a token via GetChatServer
    GetChatServerReq gsReq;
    gsReq.set_uid(4002);
    GetChatServerRsp gsRsp;
    grpc::ClientContext gsCtx;
    auto gsStatus = grpc_->stub()->GetChatServer(&gsCtx, gsReq, &gsRsp);
    ASSERT_TRUE(gsStatus.ok());

    LoginReq loginReq;
    loginReq.set_uid(4002);
    loginReq.set_token(gsRsp.token());
    LoginRsp loginRsp;
    grpc::ClientContext loginCtx;
    auto loginStatus = grpc_->stub()->Login(&loginCtx, loginReq, &loginRsp);
    EXPECT_TRUE(loginStatus.ok());
    EXPECT_EQ(loginRsp.uid(), 4002);
}

// Verify VerifyToken succeeds for a logged-in user.
TEST_F(StatusIntegrationTest, VerifyTokenValid) {
    GetChatServerReq gsReq;
    gsReq.set_uid(4003);
    GetChatServerRsp gsRsp;
    grpc::ClientContext gsCtx;
    ASSERT_TRUE(grpc_->stub()->GetChatServer(&gsCtx, gsReq, &gsRsp).ok());

    LoginReq loginReq;
    loginReq.set_uid(4003);
    loginReq.set_token(gsRsp.token());
    LoginRsp loginRsp;
    grpc::ClientContext loginCtx;
    ASSERT_TRUE(grpc_->stub()->Login(&loginCtx, loginReq, &loginRsp).ok());

    VerifyTokenReq vtReq;
    vtReq.set_uid(4003);
    vtReq.set_token(gsRsp.token());
    VerifyTokenRsp vtRsp;
    grpc::ClientContext vtCtx;
    auto vtStatus = grpc_->stub()->VerifyToken(&vtCtx, vtReq, &vtRsp);
    EXPECT_TRUE(vtStatus.ok());
    EXPECT_EQ(vtRsp.error(), 0);
}
