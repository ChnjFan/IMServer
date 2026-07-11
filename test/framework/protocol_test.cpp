#include <gtest/gtest.h>
#include "protocol.h"

TEST(ProtocolTest, EncodeDecodeRoundTrip) {
    protocol_reset_buffer();
    Json::Value body;
    body["uid"] = 123;
    body["name"] = "alice";

    std::string frame = encode(3001, body);

    auto frames = decode(frame.data(), frame.size());
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].msgId, 3001);
    EXPECT_EQ(frames[0].body["uid"].asInt(), 123);
    EXPECT_EQ(frames[0].body["name"].asString(), "alice");
}

TEST(ProtocolTest, SplitPacket) {
    protocol_reset_buffer();
    Json::Value body;
    body["msg"] = "hello world, this is a longer body for split packet testing";

    std::string frame = encode(1004, body);

    // 先送入前半段——应返回空（不完整）
    auto frames = decode(frame.data(), frame.size() / 2);
    EXPECT_EQ(frames.size(), 0u);

    // 再送入剩余部分——此时应得到一帧完整数据
    frames = decode(frame.data() + frame.size() / 2, frame.size() - frame.size() / 2);
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].body["msg"].asString(), "hello world, this is a longer body for split packet testing");
}

TEST(ProtocolTest, StickyPackets) {
    protocol_reset_buffer();
    Json::Value b1;
    b1["a"] = 1;
    Json::Value b2;
    b2["b"] = 2;

    // 两个帧首尾粘连在一个缓冲区中
    std::string combined = encode(1005, b1) + encode(1006, b2);

    auto frames = decode(combined.data(), combined.size());
    ASSERT_EQ(frames.size(), 2u);
    EXPECT_EQ(frames[0].msgId, 1005);
    EXPECT_EQ(frames[0].body["a"].asInt(), 1);
    EXPECT_EQ(frames[1].msgId, 1006);
    EXPECT_EQ(frames[1].body["b"].asInt(), 2);
}
