#include "stress_test_client.h"
#include "stress_metrics.h"
#include "protocol.h"

#include <iostream>
#include <sstream>
#include <unordered_map>

StressTestClient::StressTestClient(net::io_context& io, StressMetrics* metrics)
    : io_(io)
    , socket_(io)
    , deadline_timer_(io)
    , heartbeat_timer_(io)
    , send_timer_(io)
    , metrics_(metrics)
    , lastActive_(std::chrono::steady_clock::now()) {
}

StressTestClient::~StressTestClient() {
    close();
}

void StressTestClient::setLoginInfo(int uid, std::string token) {
    uid_ = uid;
    token_ = std::move(token);
}

void StressTestClient::setMessageHandler(MessageHandler handler) {
    onMessage_ = std::move(handler);
}

void StressTestClient::asyncConnect(const tcp::endpoint& endpoint, int timeout_ms) {
    state_.store(ClientState::CONNECTING);

    if (metrics_) metrics_->connect_attempts++;

    auto self = shared_from_this();

    deadline_timer_.expires_after(std::chrono::milliseconds(timeout_ms));
    deadline_timer_.async_wait([self](const boost::system::error_code& ec) {
        self->onTimeout(ec);
    });

    socket_.async_connect(endpoint, [self](const boost::system::error_code& ec) {
        self->onConnectDone(ec);
    });
}

void StressTestClient::onConnectDone(const boost::system::error_code& ec) {
    if (state_.load() != ClientState::CONNECTING) return;

    if (ec) {
        state_.store(ClientState::DISCONNECTED);
        if (metrics_) {
            metrics_->connect_failed++;
            if (ec == boost::asio::error::timed_out ||
                ec == boost::asio::error::connection_refused) {
                metrics_->connect_timeout++;
            }
        }
        return;
    }

    deadline_timer_.cancel();

    if (metrics_) metrics_->connect_success++;

    asyncReadHead();

    state_.store(ClientState::HANDSHAKING);
    doLogin();
}

void StressTestClient::onTimeout(const boost::system::error_code& ec) {
    if (ec == boost::asio::error::operation_aborted) return;
    if (state_.load() != ClientState::CONNECTING) return;

    socket_.close();
    state_.store(ClientState::DISCONNECTED);
    if (metrics_) {
        metrics_->connect_timeout++;
        metrics_->connect_failed++;
    }
}

void StressTestClient::doLogin() {
    Json::Value body;
    body["uid"] = std::to_string(uid_);
    body["token"] = token_;
    asyncSend(static_cast<uint16_t>(MessageID::ID_CHAT_LOGIN), body);
}

void StressTestClient::asyncSend(uint16_t msgId, const Json::Value& body) {
    std::string frame = encode(msgId, body);

    std::lock_guard<std::mutex> lock(sendMtx_);
    if (sendQueue_.size() >= MAX_SEND_QUEUE) {
        return;
    }

    sendQueue_.emplace(msgId, std::move(frame));

    if (!sending_) {
        sending_ = true;
        net::post(io_, [self = shared_from_this()] { self->asyncSendNext(); });
    }
}

void StressTestClient::sendChatMsg(int toUid, const std::string& content) {
    Json::Value body;
    const std::string convId = "c2c_" + std::to_string(std::min(uid_, toUid)) + "_" + std::to_string(std::max(uid_, toUid));
    body["from_uid"] = uid_;
    body["to_uid"] = toUid;
    body["conv_id"] = convId;
    body["content"] = content;
    body["content_type"] = 1; // 文本消息
    body["status"] = 0; // 已发送
    body["msg_id"] = chatMsgId_++; // 消息序号
    asyncSend(static_cast<uint16_t>(MessageID::ID_CHAT_MSG_REQ), body);
}

void StressTestClient::sendHeartbeat() {
    Json::Value body;
    body["uid"] = uid_;
    asyncSend(static_cast<uint16_t>(MessageID::ID_HEART_BEAT_REQ), body);
}

void StressTestClient::sendFriendApply(int toUid) {
    Json::Value body;
    body["uid"] = uid_;
    body["friend_id"] = toUid;
    body["message"] = "stress test friend apply";
    asyncSend(static_cast<uint16_t>(MessageID::ID_FRIEND_APPLY_REQ), body);
}

void StressTestClient::sendUserSearch(int uid) {
    Json::Value body;
    body["uid"] = std::to_string(uid);
    asyncSend(static_cast<uint16_t>(MessageID::ID_USER_SEARCH_REQ), body);
}

void StressTestClient::asyncSendNext() {
    std::lock_guard<std::mutex> lock(sendMtx_);
    if (sendQueue_.empty()) {
        sending_ = false;
        return;
    }

    auto& node = sendQueue_.front();
    auto self = shared_from_this();

    // 记录发送时间 (用于 RTT 测量)
    sendTimes_[node.msgId] = std::chrono::steady_clock::now();

    boost::asio::async_write(socket_, boost::asio::buffer(node.data),
        [self, msgId = node.msgId](const boost::system::error_code& ec, size_t bytes) {
            if (ec) {
                self->close();
                return;
            }
            if (self->metrics_) self->metrics_->msg_sent++;

            // per-type sent counting
            if (self->metrics_) {
                if (msgId == static_cast<uint16_t>(MessageID::ID_CHAT_MSG_REQ))
                    self->metrics_->chat_msg_sent++;
                else if (msgId == static_cast<uint16_t>(MessageID::ID_FRIEND_APPLY_REQ))
                    self->metrics_->friend_apply_sent++;
                else if (msgId == static_cast<uint16_t>(MessageID::ID_USER_SEARCH_REQ))
                    self->metrics_->user_search_sent++;
            }

            {
                std::lock_guard<std::mutex> lock(self->sendMtx_);
                self->sendQueue_.pop();
            }
            self->asyncSendNext();
        });
}

void StressTestClient::recordRtt(uint16_t msgId) {
    auto it = sendTimes_.find(msgId-1);
    if (it == sendTimes_.end()) return;

    auto rtt = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - it->second).count();

    if (metrics_) {
        metrics_->rtt_hist.record(rtt);
    }

    sendTimes_.erase(it);
}

void StressTestClient::asyncReadHead() {
    auto self = shared_from_this();
    asyncReadSome(0, HEAD_TOTAL_LEN,
        [self](const boost::system::error_code& ec, uint16_t bytes) {
            self->onReadHeadDone(ec, bytes);
        });
}

void StressTestClient::asyncReadSome(uint16_t readLen, uint16_t totalLen,
                                     const std::function<void(const boost::system::error_code&, uint16_t)>& cb) {
    auto self = shared_from_this();
    socket_.async_read_some(
        net::buffer(recvBuffer_ + readLen, totalLen - readLen),
        [self, readLen, totalLen, cb](const boost::system::error_code& ec, size_t bytes) {
            if (ec) {
                self->close();
                return;
            }
            uint16_t newLen = readLen + static_cast<uint16_t>(bytes);
            if (newLen >= totalLen) {
                cb(ec, newLen);
            } else {
                self->asyncReadSome(newLen, totalLen, cb);
            }
        });
}

void StressTestClient::onReadHeadDone(const boost::system::error_code& ec, uint16_t bytes) {
    if (ec || bytes < HEAD_TOTAL_LEN) {
        close();
        return;
    }

    uint16_t msgId = 0, bodyLen = 0;
    memcpy(&msgId, recvBuffer_, HEAD_MSG_ID_LEN);
    msgId = net::detail::socket_ops::network_to_host_short(msgId);
    memcpy(&bodyLen, recvBuffer_ + HEAD_MSG_ID_LEN, HEAD_MSG_SIZE_LEN);
    bodyLen = net::detail::socket_ops::network_to_host_short(bodyLen);

    if (msgId >= static_cast<uint16_t>(MessageID::INVALID_ID) || bodyLen > MAX_BUFFER_SIZE) {
        close();
        return;
    }

    currentMsgId_ = msgId;
    currentBodyLen_ = bodyLen;

    auto self = shared_from_this();
    asyncReadSome(0, bodyLen,
        [self, bodyLen](const boost::system::error_code& ec, uint16_t bytes) {
            self->onReadBodyDone(ec, bytes, bodyLen);
        });
}

void StressTestClient::onReadBodyDone(const boost::system::error_code& ec, uint16_t bytes, uint16_t bodyLen) {
    if (ec || bytes < bodyLen) {
        close();
        return;
    }

    Json::Value body;
    Json::CharReaderBuilder reader;
    std::string bodyStr(recvBuffer_, bodyLen);
    std::istringstream bodyStream(bodyStr);
    std::string errs;
    if (Json::parseFromStream(reader, bodyStream, &body, &errs)) {
        if (metrics_) metrics_->msg_recv++;
        // per-type recv counting
        if (metrics_) {
            if (currentMsgId_ == static_cast<uint16_t>(MessageID::ID_CHAT_MSG_RSP))
                metrics_->chat_msg_recv++;
            else if (currentMsgId_ == static_cast<uint16_t>(MessageID::ID_FRIEND_APPLY_RSP))
                metrics_->friend_apply_recv++;
            else if (currentMsgId_ == static_cast<uint16_t>(MessageID::ID_USER_SEARCH_RSP))
                metrics_->user_search_recv++;
        }
        handleMessage(currentMsgId_, body);
    }

    asyncReadHead();
}

void StressTestClient::handleMessage(uint16_t msgId, const Json::Value& body) {
    recordRtt(msgId);
    if (msgId == static_cast<uint16_t>(MessageID::ID_CHAT_LOGIN_RSP)) {
        int error = body.isMember("error") ? body["error"].asInt() : -1;

        if (error == 0) {
            state_.store(ClientState::ONLINE);
            if (metrics_) {
                metrics_->handshake_success++;
                metrics_->current_online++;
                int64_t current = metrics_->current_online.load();
                int64_t peak = metrics_->peak_online.load();
                while (current > peak && !metrics_->peak_online.compare_exchange_weak(peak, current)) {}
            }
            scheduleHeartbeat();
        } else {
            state_.store(ClientState::DISCONNECTED);
            if (metrics_) metrics_->handshake_failed++;
        }
        return;
    }

    if (msgId == static_cast<uint16_t>(MessageID::ID_HEART_BEAT_RSP)) {
        updateActiveTime();
        return;
    }

    if (onMessage_) {
        onMessage_(msgId, body);
    }
}

void StressTestClient::scheduleHeartbeat() {
    if (state_.load() != ClientState::ONLINE) return;

    heartbeat_timer_.expires_after(std::chrono::seconds(HEARTBEAT_INTERVAL_S));
    heartbeat_timer_.async_wait([self = shared_from_this()](const boost::system::error_code& ec) {
        if (ec == boost::asio::error::operation_aborted) return;
        if (self->state_.load() != ClientState::ONLINE) return;
        self->sendHeartbeat();
        self->scheduleHeartbeat();
    });
}

void StressTestClient::updateActiveTime() {
    lastActive_ = std::chrono::steady_clock::now();
}

void StressTestClient::startMsgRate(int msg_per_sec, int min_uid, int max_uid) {
    if (msg_per_sec <= 0) return;
    msg_rate_per_sec_.store(msg_per_sec);
    target_min_uid_.store(min_uid);
    target_max_uid_.store(max_uid);
    scheduleSend();
}

void StressTestClient::stopMsgRate() {
    msg_rate_per_sec_.store(0);
    send_timer_.cancel();
    mixed_mode_.store(false);
}

void StressTestClient::startMixedMsgRate(int msg_per_sec, int min_uid, int max_uid,
                                         float chat_ratio, float friend_ratio, float query_ratio) {
    if (msg_per_sec <= 0) return;
    msg_rate_per_sec_.store(msg_per_sec);
    target_min_uid_.store(min_uid);
    target_max_uid_.store(max_uid);
    chat_ratio_.store(chat_ratio);
    friend_ratio_.store(friend_ratio);
    query_ratio_.store(query_ratio);
    mixed_mode_.store(true);
    scheduleSend();
}

void StressTestClient::scheduleSend() {
    int rate = msg_rate_per_sec_.load();
    if (rate <= 0) return;

    auto self = shared_from_this();
    // 在 0.5/rate 和 1.5/rate 之间随机化间隔，避免所有客户端同步发送
    std::uniform_real_distribution<double> jitter(0.5, 1.5);
    double interval_ms = (1000.0 / rate) * jitter(rng_);

    send_timer_.expires_after(std::chrono::microseconds(static_cast<int64_t>(interval_ms * 1000)));
    send_timer_.async_wait([self](const boost::system::error_code& ec) {
        if (ec == boost::asio::error::operation_aborted) return;
        self->sendTimerHandler();
    });
}

void StressTestClient::sendTimerHandler() {
    if (state_.load() != ClientState::ONLINE) return;
    int rate = msg_rate_per_sec_.load();
    if (rate <= 0) return;

    int minUid = target_min_uid_.load();
    int maxUid = target_max_uid_.load();
    if (maxUid > minUid) {
        std::uniform_int_distribution<> uidDist(minUid, maxUid);
        int toUid = uidDist(rng_);
        if (toUid == uid_) {
            toUid = (toUid + 1 > maxUid) ? toUid - 1 : toUid + 1;
        }

        if (mixed_mode_.load()) {
            // 按权重概率选择消息类型
            std::uniform_real_distribution<float> typeDist(0.0f, 1.0f);
            float r = typeDist(rng_);
            float chatR = chat_ratio_.load();
            float friendR = friend_ratio_.load();
            if (r < chatR) {
                sendChatMsg(toUid, "throughput");
            } else if (r < chatR + friendR) {
                sendFriendApply(toUid);
            } else {
                sendUserSearch(toUid);
            }
        } else {
            sendChatMsg(toUid, "throughput");
        }
    }

    // 重新调度
    scheduleSend();
}



void StressTestClient::close() {
    // 使用 exchange 原子操作获取之前的状态
    // 只有第一个将状态从非 DISDISCONNECTED 切换到 DISCONNECTED 的线程会执行清理和计数
    // 后续重复调用会直接返回，避免 disconnect_ 重复计数
    ClientState prev = state_.exchange(ClientState::DISCONNECTED);
    if (prev == ClientState::DISCONNECTED) return;

    deadline_timer_.cancel();
    heartbeat_timer_.cancel();

    boost::system::error_code ec;
    socket_.shutdown(tcp::socket::shutdown_both, ec);
    socket_.close(ec);

    if (prev == ClientState::ONLINE && metrics_) {
        metrics_->current_online--;
        metrics_->disconnect_++;
    }
}
