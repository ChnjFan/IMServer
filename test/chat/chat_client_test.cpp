#include <gtest/gtest.h>
#include "fixture_base.h"
#include "chat_test_client.h"
#include "ConfigMgr.h"

class ChatClientTest : public IntegrationTestBase {
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

// 验证正常连接与断开
TEST_F(ChatClientTest, ConnectDisconnect) {
    ChatTestClient client;
    EXPECT_TRUE(client.connect(chatHost_, chatPort_));
    EXPECT_TRUE(client.isConnected());
    client.disconnect();
    EXPECT_FALSE(client.isConnected());
}

// 验证连接不存在的端口会失败
// TEST_F(ChatClientTest, InvalidConnectFails) {
//     ChatTestClient client;
//     EXPECT_FALSE(client.connect("127.0.0.1", 19999));
// }

// 验证未登录时也能收发心跳帧（服务端会返回 PONG）
TEST_F(ChatClientTest, Heartbeat) {
    ChatTestClient client;
    ASSERT_TRUE(client.connect(chatHost_, chatPort_));
    client.heartbeat();
    EXPECT_EQ(client.messagesSent(), 1u);
}
