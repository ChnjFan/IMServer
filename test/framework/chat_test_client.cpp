#include "chat_test_client.h"
#include "protocol.h"
#include "const.h"
#include <iostream>

#define MAX_SEND_QUEUE 1024

ChatTestClient::~ChatTestClient() {
    close();
}

void ChatTestClient::start() {
    auto self = shared_from_this();
    // 异步连接服务端
    const boost::asio::ip::address addr = boost::asio::ip::make_address(host_);
    socket_.async_connect(
        tcp::endpoint(addr, port_),
        [self](boost::system::error_code ec) {
            if (!ec) {
                self->login(); // 连接成功，发登录消息
            } else {
                std::cerr << "connect fail: " << ec.message() << std::endl;
            }
        }
    );
}

void ChatTestClient::close() {
    socket_.close();
}

void ChatTestClient::recordMetrics()
{
    auto end = std::chrono::steady_clock::now();
    metrics_->throughput.tick();
    auto rtt_us = std::chrono::duration_cast<std::chrono::microseconds>(end - beginTime_).count();
    metrics_->latency.record(rtt_us);
    metrics_->errors.addSuccess();
}

bool ChatTestClient::sendChatMsg(int toUid, const std::string& content) {
    Json::Value body;
    const std::string convId = "c2c_" + std::to_string(std::min(uid_, toUid)) + "_" + std::to_string(std::max(uid_, toUid));
    body["from_uid"] = uid_;
    body["to_uid"] = toUid;
    body["conv_id"] = convId;
    body["content"] = content;
    body["content_type"] = 1; // 文本消息
    body["status"] = 0; // 已发送
    body["msg_id"] = sent_ + 1; // 消息序号
    asyncSend(static_cast<uint16_t>(MessageID::ID_CHAT_MSG_REQ), body);
    return true;
}

bool ChatTestClient::heartbeat() {
    Json::Value body;
    body["uid"] = 1;
    asyncSend(static_cast<uint16_t>(MessageID::ID_HEART_BEAT_REQ), body);
    return true;
}

void ChatTestClient::login() {
    Json::Value body;
    // 服务端对 uid 调用 asString() + stoi，序列化为字符串更稳妥
    body["uid"] = std::to_string(uid_);
    body["token"] = token_;
    asyncSend(static_cast<uint16_t>(MessageID::ID_CHAT_LOGIN), body);
}

void ChatTestClient::asyncSend() {
    const auto& node = sendNodeQueue_.front();
    auto self = shared_from_this();
    serialize_end_time_ = std::chrono::steady_clock::now();
    beginTime_ = serialize_end_time_;

    boost::asio::async_write(socket_, boost::asio::buffer(node->buffer_, node->used_),
        [self, this](const boost::system::error_code& error, size_t bytes_transfer) {
            if (error) {
                errors_++;
                close();
                return;
            }

            sent_++;
            sendNodeQueue_.pop();
        });
}

void ChatTestClient::asyncSend(uint16_t msgId, const Json::Value &body)
{
    serialize_start_time_ = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(sendMtx_);
    const size_t sendSize = sendNodeQueue_.size();
    if (sendSize > MAX_SEND_QUEUE) {   // 发送抑制
        return;
    }

    std::string msg = body.toStyledString();
    sendNodeQueue_.push(std::make_shared<SendNode>(msg.c_str(), msg.size(), msgId));
    if (sendSize > 0) {
        return; // 已经有线程在发送数据，不需要重复调用发送
    }
    asyncSend();
    asyncRecv();    // 发完消息后等待接收
}

void ChatTestClient::asyncRecv() {
    auto self = shared_from_this();
    asyncReadFull(HEAD_TOTAL_LEN, [self, this](const boost::system::error_code& ec, const uint16_t bytes_transfer) {
        try {
            if (ec || bytes_transfer != HEAD_TOTAL_LEN) {
                errors_++;
                close();
                return;
            }

            char node[HEAD_TOTAL_LEN+1] = {0};

            memcpy(node, buffer_, HEAD_TOTAL_LEN);
            // 获取头部数据
            uint16_t msgId = 0;
            memcpy(&msgId, node, HEAD_MSG_ID_LEN);
            msgId = net::detail::socket_ops::network_to_host_short(msgId);

            uint16_t msgLen = 0;
            memcpy(&msgLen, node + HEAD_MSG_ID_LEN, HEAD_MSG_SIZE_LEN);
            msgLen = net::detail::socket_ops::network_to_host_short(msgLen);

            asyncReadBody(msgLen);
        } catch (std::exception& e) {
            std::cout << e.what() << std::endl;
        }
    });
}

void ChatTestClient::asyncReadBody(uint16_t size) {
    auto self = shared_from_this();
    asyncReadFull(size, [self, this, size](const boost::system::error_code& ec, const uint16_t bytes_transfer) {
        if (ec || bytes_transfer < size) {
            errors_++;
            close();
            return;
        }
        
        recv_++;
        readCallback_(shared_from_this());
    });
}

void ChatTestClient::asyncReadFull(std::uint16_t totalLen, 
                    const std::function<void(const boost::system::error_code &, std::uint16_t)> &callback)
{
    memset(buffer_, 0, sizeof(buffer_));
    asyncReadSome(0, totalLen, callback);
}

void ChatTestClient::asyncReadSome(std::uint16_t readLen, std::uint16_t totalLen,
                    const std::function<void(const boost::system::error_code &, std::uint16_t)> &callback)
{
    auto self = shared_from_this();
    socket_.async_read_some(boost::asio::buffer(buffer_ + readLen, totalLen - readLen),
        [self, readLen, totalLen, callback](const boost::system::error_code &ec, const std::uint16_t bytes_transfer) {
            if (ec || readLen + bytes_transfer >= totalLen) {   // 读出错或者已经读到想要的长度后返回
                callback(ec, readLen + bytes_transfer); // 返回 buffer 中已经读取的内容长度
                return;
            }
            // 没有读完继续读
            self->asyncReadSome(readLen + bytes_transfer, totalLen, callback);
    });
}
