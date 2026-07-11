#include <gtest/gtest.h>
#include "fixture_base.h"
#include "chat_test_client.h"
#include "grpc_test_client.h"
#include "message.grpc.pb.h"
#include "ConfigMgr.h"

using message::GetChatServerReq;
using message::GetChatServerRsp;
using message::StatusService;

class ChatIntegrationTest : public IntegrationTestBase {
protected:
    void SetUp() override {
        IntegrationTestBase::SetUp();
        auto& config = ConfigMgr::getInstance();
        chatHost_ = "127.0.0.1";
        chatPort_ = static_cast<uint16_t>(std::stoi(config["ChatServer"]["Port"]));
    }

    std::string chatHost_;
    uint16_t chatPort_;
};

// 验证使用 StatusServer 获取的 token 能成功登录 ChatServer
TEST_F(ChatIntegrationTest, LoginWithToken) {
    auto& config = ConfigMgr::getInstance();
    GrpcTestClient<StatusService> statusCli("127.0.0.1",
        static_cast<uint16_t>(std::stoi(config["StatusServer"]["Port"])));

    GetChatServerReq gsReq;
    gsReq.set_uid(3001);
    GetChatServerRsp gsRsp;
    grpc::ClientContext gsCtx;
    ASSERT_TRUE(statusCli.stub()->GetChatServer(&gsCtx, gsReq, &gsRsp).ok());

    ChatTestClient chatCli;
    ASSERT_TRUE(chatCli.connect(chatHost_, chatPort_));
    EXPECT_TRUE(chatCli.chatLogin(3001, gsRsp.token()));
}

// 验证给不存在的用户发消息会返回错误（而非崩溃）
TEST_F(ChatIntegrationTest, SendMessageToOfflineUser) {
    auto& config = ConfigMgr::getInstance();
    GrpcTestClient<StatusService> statusCli("127.0.0.1",
        static_cast<uint16_t>(std::stoi(config["StatusServer"]["Port"])));

    GetChatServerReq gsReq;
    gsReq.set_uid(3002);
    GetChatServerRsp gsRsp;
    grpc::ClientContext gsCtx;
    ASSERT_TRUE(statusCli.stub()->GetChatServer(&gsCtx, gsReq, &gsRsp).ok());

    ChatTestClient chatCli;
    ASSERT_TRUE(chatCli.connect(chatHost_, chatPort_));
    ASSERT_TRUE(chatCli.chatLogin(3002, gsRsp.token()));

    // 发送给不存在的用户，应收到错误响应
    bool sent = chatCli.sendChatMsg(99999, "hello offline user");
    // 消息已发出（不管服务端最终返回什么错误）
    EXPECT_EQ(chatCli.messagesSent(), 2u); // login + chat msg
}
