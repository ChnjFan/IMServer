#ifndef IMSERVER_CHAT_TEST_CLIENT_H
#define IMSERVER_CHAT_TEST_CLIENT_H

#include <chrono>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <atomic>
#include <memory>
#include <json/json.h>

#include "const.h"
#include "metrics.h"
#include "account_manager.h"

namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

struct PendingResponse {
    std::function<bool(uint16_t, const Json::Value&)> matcher;
    std::promise<std::optional<Json::Value>> promise;
};

class SendNode {
public:
    uint16_t msgId_;
    uint16_t used_;
    uint16_t capacity_;
    char *buffer_;

    SendNode(const char* msg, uint16_t size, uint16_t msgId) 
        : msgId_(msgId), used_(0), capacity_(size + HEAD_TOTAL_LEN + 1), buffer_(new char[capacity_]) {
        const uint16_t msgIdHost = net::detail::socket_ops::host_to_network_short(msgId);
        memcpy(buffer_, &msgIdHost, HEAD_MSG_ID_LEN);
        const uint16_t msgSizeHost = net::detail::socket_ops::host_to_network_short(size);
        memcpy(buffer_ + HEAD_MSG_ID_LEN, &msgSizeHost, HEAD_MSG_SIZE_LEN);
        if (msg) {
            memcpy(buffer_ + HEAD_TOTAL_LEN, msg, size);
        }
        used_ = size + HEAD_TOTAL_LEN;
    }

    ~SendNode() {
        delete[] buffer_;
    }
};

class ChatTestClient : public std::enable_shared_from_this<ChatTestClient> {
public:
    using MessageHandler = std::function<void(uint16_t msgId, const Json::Value& body)>;

    ~ChatTestClient();

    ChatTestClient(std::shared_ptr<Metrics> metrics, boost::asio::io_context& io_context,
        const std::function<void(std::shared_ptr<ChatTestClient>)> &readCallback)
        : port_(0), buffer_{0}, readCallback_(readCallback), socket_(io_context), metrics_(metrics) {
    };

    void start();
    void close();

    void recordMetrics();

    void setAccount(const TestAccount& acct) {
        uid_ = acct.uid;
        token_ = acct.token;
        host_ = acct.host;
        port_ = acct.port;
    }

    bool sendChatMsg(int toUid, const std::string& content);
    bool heartbeat();

    uint64_t messagesSent() const { return sent_; }
    uint64_t messagesReceived() const { return recv_; }
    uint64_t errors() const { return errors_; }

private:
    void login();

    void asyncSend();
    void asyncSend(uint16_t msgId, const Json::Value& body);

    void asyncRecv();
    void asyncReadBody(uint16_t size);
    void asyncReadFull(std::uint16_t totalLen,
        const std::function<void(const boost::system::error_code&, std::uint16_t)>& callback);
    void asyncReadSome(std::uint16_t readLen, std::uint16_t totalLen,
        const std::function<void(const boost::system::error_code &, std::uint16_t)>& callback);

    std::string host_;
    uint16_t port_;

    std::mutex sendMtx_;
    std::queue<std::shared_ptr<SendNode>> sendNodeQueue_;

    std::shared_ptr<Metrics> metrics_;
    std::chrono::steady_clock::time_point beginTime_ = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point serialize_start_time_;
    std::chrono::steady_clock::time_point serialize_end_time_;

    char buffer_[MAX_BUFFER_SIZE];

    int uid_ = 0;
    std::string email_;
    std::string token_;

    std::function<void(std::shared_ptr<ChatTestClient>)> readCallback_;

    tcp::socket socket_;

    std::atomic<uint64_t> sent_{0};
    std::atomic<uint64_t> recv_{0};
    std::atomic<uint64_t> errors_{0};
};

#endif
