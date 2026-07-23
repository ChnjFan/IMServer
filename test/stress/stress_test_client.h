#ifndef IMSERVER_STRESS_TEST_CLIENT_H
#define IMSERVER_STRESS_TEST_CLIENT_H

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <random>
#include <string>

#include <boost/asio.hpp>
#include <json/json.h>

#include "const.h"

namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

class StressMetrics;

enum class ClientState : uint8_t {
    DISCONNECTED = 0,
    CONNECTING,
    HANDSHAKING,
    ONLINE,
};

class StressTestClient : public std::enable_shared_from_this<StressTestClient> {
public:
    using Ptr = std::shared_ptr<StressTestClient>;
    using MessageHandler = std::function<void(uint16_t msgId, const Json::Value& body)>;

    StressTestClient(net::io_context& io, StressMetrics* metrics);
    ~StressTestClient();

    StressTestClient(const StressTestClient&) = delete;
    StressTestClient& operator=(const StressTestClient&) = delete;

    void asyncConnect(const tcp::endpoint& endpoint, int timeout_ms = 5000);
    void asyncSend(uint16_t msgId, const Json::Value& body);
    void sendChatMsg(int toUid, const std::string& content);
    void sendHeartbeat();
    void sendFriendApply(int toUid);
    void sendUserSearch(int uid);
    void close();
    void setLoginInfo(int uid, std::string token);
    void setMessageHandler(MessageHandler handler);

    /** @brief 启动定频消息发送 (msg_per_sec 条/秒, 目标 uid 范围为 [min_uid, max_uid]) */
    void startMsgRate(int msg_per_sec, int min_uid, int max_uid);
    /** @brief 停止定频消息发送 */
    void stopMsgRate();
    /** @brief 启动混合消息定频发送 (聊天/好友申请/用户搜索按比例混合) */
    void startMixedMsgRate(int msg_per_sec, int min_uid, int max_uid,
                           float chat_ratio, float friend_ratio, float query_ratio);

    ClientState state() const { return state_.load(); }
    int uid() const { return uid_; }

private:
    void asyncReadHead();
    void asyncReadSome(uint16_t readLen, uint16_t totalLen,
                       const std::function<void(const boost::system::error_code&, uint16_t)>& cb);

    void asyncSendNext();
    void scheduleHeartbeat();
    void recordRtt(uint16_t msgId);

    void onConnectDone(const boost::system::error_code& ec);
    void onTimeout(const boost::system::error_code& ec);
    void onReadHeadDone(const boost::system::error_code& ec, uint16_t bytes);
    void onReadBodyDone(const boost::system::error_code& ec, uint16_t bytes, uint16_t bodyLen);

    void doLogin();
    void handleMessage(uint16_t msgId, const Json::Value& body);
    void updateActiveTime();
    void sendTimerHandler();
    void scheduleSend();

    static constexpr int MAX_SEND_QUEUE = 1024;
    static constexpr int HEARTBEAT_INTERVAL_S = 30;

    net::io_context& io_;
    tcp::socket socket_;
    net::steady_timer deadline_timer_;
    net::steady_timer heartbeat_timer_;
    net::steady_timer send_timer_;

    std::atomic<ClientState> state_{ClientState::DISCONNECTED};
    int uid_ = 0;
    std::string token_;

    struct SendNode {
        uint16_t msgId;
        std::string data;
        SendNode(uint16_t id, std::string d) : msgId(id), data(std::move(d)) {}
    };
    std::queue<SendNode> sendQueue_;
    std::mutex sendMtx_;
    bool sending_ = false;
    int chatMsgId_ = 0;

    char recvBuffer_[MAX_BUFFER_SIZE];
    uint16_t currentMsgId_ = 0;
    uint16_t currentBodyLen_ = 0;

    MessageHandler onMessage_;
    StressMetrics* metrics_ = nullptr;
    std::chrono::steady_clock::time_point lastActive_;

    // RTT 测量: 记录每种消息类型的最后发送时间
    std::unordered_map<uint16_t, std::chrono::steady_clock::time_point> sendTimes_;

    // 定频发送控制
    std::atomic<int> msg_rate_per_sec_{0};
    std::atomic<int> target_min_uid_{0};
    std::atomic<int> target_max_uid_{0};
    std::mt19937 rng_{std::random_device{}()};

    // 混合消息发送控制
    std::atomic<bool> mixed_mode_{false};
    std::atomic<float> chat_ratio_{0.7f};
    std::atomic<float> friend_ratio_{0.2f};
    std::atomic<float> query_ratio_{0.1f};
};

#endif // IMSERVER_STRESS_TEST_CLIENT_H
