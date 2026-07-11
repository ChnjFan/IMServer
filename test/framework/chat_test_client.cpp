#include "chat_test_client.h"
#include "protocol.h"
#include "const.h"
#include <iostream>

ChatTestClient::ChatTestClient() = default;

ChatTestClient::~ChatTestClient() {
    disconnect();
}

bool ChatTestClient::connect(const std::string& host, uint16_t port, int timeoutMs) {
    try {
        tcp::resolver resolver(ioc_);
        auto endpoints = resolver.resolve(host, std::to_string(port));
        socket_ = std::make_unique<tcp::socket>(ioc_);

        boost::system::error_code ec;
        boost::asio::connect(*socket_, endpoints, ec);
        if (ec) {
            errors_++;
            return false;
        }

        connected_ = true;
        stop_ = false;
        recvThread_ = std::make_unique<std::thread>(&ChatTestClient::recvLoop, this);
        return true;
    } catch (const std::exception& e) {
        errors_++;
        return false;
    }
}

void ChatTestClient::disconnect() {
    stop_ = true;
    connected_ = false;
    if (socket_ && socket_->is_open()) {
        boost::system::error_code ec;
        socket_->close(ec);
    }
    if (recvThread_ && recvThread_->joinable()) {
        recvThread_->join();
    }
}

bool ChatTestClient::isConnected() const {
    return connected_;
}

bool ChatTestClient::send(uint16_t msgId, const Json::Value& body) {
    if (!connected_) return false;
    std::string frame = encode(msgId, body);
    boost::system::error_code ec;
    boost::asio::write(*socket_, boost::asio::buffer(frame), ec);
    if (ec) {
        errors_++;
        connected_ = false;
        return false;
    }
    sent_++;
    return true;
}

std::optional<Json::Value> ChatTestClient::sendAndWait(
    uint16_t msgId, const Json::Value& body,
    std::function<bool(uint16_t, const Json::Value&)> match,
    int timeoutMs) {

    auto pending = std::make_shared<PendingResponse>();
    pending->matcher = match;
    auto future = pending->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(pendingMtx_);
        pending_.push(pending);
    }

    if (!send(msgId, body)) {
        std::lock_guard<std::mutex> lock(pendingMtx_);
        pending_.pop();
        return std::nullopt;
    }

    if (future.wait_for(std::chrono::milliseconds(timeoutMs)) == std::future_status::timeout) {
        errors_++;
        return std::nullopt;
    }
    return future.get();
}

void ChatTestClient::onMessage(uint16_t msgId, MessageHandler handler) {
    std::lock_guard<std::mutex> lock(handlerMtx_);
    handlers_[msgId] = handler;
}

bool ChatTestClient::chatLogin(int uid, const std::string& token) {
    Json::Value body;
    // 服务端对 uid 调用 asString() + stoi，序列化为字符串更稳妥
    body["uid"] = std::to_string(uid);
    body["token"] = token;
    auto rsp = sendAndWait(static_cast<uint16_t>(MessageID::ID_CHAT_LOGIN), body,
        [](uint16_t id, const Json::Value&) {
            return id == static_cast<uint16_t>(MessageID::ID_CHAT_LOGIN_RSP);
        });
    return rsp.has_value() && rsp->isMember("error") && (*rsp)["error"].asInt() == 0;
}

bool ChatTestClient::sendChatMsg(int toUid, const std::string& content) {
    Json::Value body;
    body["touid"] = toUid;
    body["msg"] = content;
    body["msgid"] = 1; // 文本消息
    auto rsp = sendAndWait(static_cast<uint16_t>(MessageID::ID_CHAT_MSG_REQ), body,
        [](uint16_t id, const Json::Value&) {
            return id == static_cast<uint16_t>(MessageID::ID_CHAT_MSG_RSP);
        });
    return rsp.has_value() && (*rsp)["error"].asInt() == 0;
}

bool ChatTestClient::heartbeat() {
    Json::Value body;
    auto rsp = sendAndWait(static_cast<uint16_t>(MessageID::ID_HEART_BEAT_REQ), body,
        [](uint16_t id, const Json::Value&) {
            return id == static_cast<uint16_t>(MessageID::ID_HEART_BEAT_RSP);
        });
    return rsp.has_value();
}

void ChatTestClient::recvLoop() {
    char buf[4096];
    while (!stop_) {
        boost::system::error_code ec;
        size_t n = socket_->read_some(boost::asio::buffer(buf, sizeof(buf)), ec);
        if (ec) {
            connected_ = false;
            errors_++;
            break;
        }
        auto frames = decode(buf, n);
        for (auto& f : frames) {
            handleFrame(f.msgId, f.body);
        }
    }
}

void ChatTestClient::handleFrame(uint16_t msgId, const Json::Value& body) {
    recv_++;

    // 优先分发给匹配的待响应请求
    {
        std::lock_guard<std::mutex> lock(pendingMtx_);
        if (!pending_.empty()) {
            auto& front = pending_.front();
            if (front->matcher(msgId, body)) {
                front->promise.set_value(body);
                pending_.pop();
                return;
            }
        }
    }

    // 否则分发给已注册的异步回调
    {
        std::lock_guard<std::mutex> lock(handlerMtx_);
        auto it = handlers_.find(msgId);
        if (it != handlers_.end()) {
            it->second(msgId, body);
        }
    }
}
