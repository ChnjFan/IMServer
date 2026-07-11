#ifndef IMSERVER_CHAT_TEST_CLIENT_H
#define IMSERVER_CHAT_TEST_CLIENT_H

#include <cstdint>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <atomic>
#include <memory>
#include <boost/asio.hpp>
#include <json/json.h>

namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

struct PendingResponse {
    std::function<bool(uint16_t, const Json::Value&)> matcher;
    std::promise<std::optional<Json::Value>> promise;
};

class ChatTestClient {
public:
    using MessageHandler = std::function<void(uint16_t msgId, const Json::Value& body)>;

    ChatTestClient();
    ~ChatTestClient();

    bool connect(const std::string& host, uint16_t port, int timeoutMs = 5000);
    void disconnect();
    bool isConnected() const;

    std::optional<Json::Value> sendAndWait(
        uint16_t msgId, const Json::Value& body,
        std::function<bool(uint16_t, const Json::Value&)> match,
        int timeoutMs = 5000);

    bool send(uint16_t msgId, const Json::Value& body);
    void onMessage(uint16_t msgId, MessageHandler handler);

    bool chatLogin(int uid, const std::string& token);
    bool sendChatMsg(int toUid, const std::string& content);
    bool heartbeat();

    uint64_t messagesSent() const { return sent_; }
    uint64_t messagesReceived() const { return recv_; }
    uint64_t errors() const { return errors_; }

private:
    void recvLoop();
    void handleFrame(uint16_t msgId, const Json::Value& body);

    std::atomic<bool> connected_{false};
    std::atomic<bool> stop_{false};
    net::io_context ioc_;
    std::unique_ptr<tcp::socket> socket_;
    std::unique_ptr<std::thread> recvThread_;
    std::string recvBuffer_;

    std::mutex pendingMtx_;
    std::queue<std::shared_ptr<PendingResponse>> pending_;

    std::mutex handlerMtx_;
    std::unordered_map<uint16_t, MessageHandler> handlers_;

    std::atomic<uint64_t> sent_{0};
    std::atomic<uint64_t> recv_{0};
    std::atomic<uint64_t> errors_{0};
};

#endif
