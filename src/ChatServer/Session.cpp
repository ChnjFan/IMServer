//
// Created by Fan on 2026/5/11.
//

#include <iostream>

#include <boost/uuid.hpp>

#include "Session.h"

#include "ChatLogicSystem.h"
#include "ChatServer.h"

using boost::uuids::uuid;
using boost::uuids::random_generator;

Session::Session(net::io_context &io_context, const std::shared_ptr<ChatServer> &chatServer)
    : stop_(false), uid_(0), io_context_(io_context), socket_(io_context), chatServer_(chatServer), buffer_{} {
    random_generator generator;
    sessionId_ = boost::uuids::to_string(generator());
    headNode_ = std::make_shared<MsgNode>(HEAD_TOTAL_LEN);
}

Session::~Session() {
    close();
}

void Session::start() {
    asyncReadHead(HEAD_TOTAL_LEN);
}

void Session::close() {
    socket_.close();
    stop_.store(true);
}

tcp::socket & Session::getSocket() {
    return socket_;
}

std::string Session::getSessionId() {
    return sessionId_;
}

void Session::setUserId(int uid) {
    uid_ = uid;
}

int Session::getUserId() {
    return uid_;
}

void Session::asyncSend(const std::string &msg, const std::uint16_t msgId) {
    asyncSend(msg.c_str(), msg.size(), msgId);
}

void Session::asyncSend(const char *msg, std::uint16_t size, std::uint16_t msgId) {
    std::lock_guard<std::mutex> lock(sendMtx_);
    const int sendSize = sendNodeQueue_.size();
    if (sendSize > MAX_SEND_QUEUE) {   // 发送抑制
        std::cout << "Session: " << sessionId_ << "Send queue is full " << MAX_SEND_QUEUE << std::endl;
        return;
    }

    sendNodeQueue_.push(std::make_shared<SendNode>(msg, size, msgId));
    if (sendSize > 0) {
        return; // 已经有线程在发送数据，不需要重复调用发送
    }
    asyncSend();
}

void Session::asyncReadHead(const std::uint16_t totalLen) {
    auto self = shared_from_this();
    asyncReadFull(totalLen, [self, this](const boost::system::error_code& ec, const uint16_t bytes_transfer) {
        try {
            if (ec || bytes_transfer != HEAD_TOTAL_LEN) {
                std::cout << "Chat server read failed, read " << bytes_transfer << ", error: " << ec.what() << std::endl;
                close();
                chatServer_->clearSession(sessionId_);
                return;
            }

            headNode_->clear();
            memcpy(headNode_->buffer_, buffer_, HEAD_TOTAL_LEN);
            headNode_->used_ = HEAD_TOTAL_LEN;

            // 获取头部数据
            uint16_t msgId = 0;
            memcpy(&msgId, headNode_->buffer_, HEAD_MSG_ID_LEN);
            msgId = net::detail::socket_ops::network_to_host_short(msgId);
            if (msgId >= static_cast<uint16_t>(MessageID::INVALID_ID)) {
                std::cout << "Invalid msg id: " << msgId << std::endl;
                chatServer_->clearSession(sessionId_);
                return;
            }
            uint16_t msgLen = 0;
            memcpy(&msgLen, headNode_->buffer_ + HEAD_MSG_ID_LEN, HEAD_MSG_SIZE_LEN);
            msgLen = net::detail::socket_ops::network_to_host_short(msgLen);
            if (msgLen > MAX_BUFFER_SIZE) {
                std::cout << "Invalid msg len: " << msgLen << std::endl;
                chatServer_->clearSession(sessionId_);
                return;
            }

            std::cout << "Recv msg head id: " << msgId << " len: " << msgLen << std::endl;
            recvNode_ = std::make_shared<RecvNode>(msgLen, msgId);
            asyncReadBody(msgLen);
        } catch (std::exception& e) {
            std::cout << e.what() << std::endl;
        }
    });
}

void Session::asyncReadBody(std::uint16_t size) {
    auto self = shared_from_this();
    asyncReadFull(size, [self, this, size](const boost::system::error_code& ec, const uint16_t bytes_transfer) {
        if (ec || bytes_transfer < size) {
            std::cout << "Read failed error: " << ec.what();
            std::cout << " or read [" << bytes_transfer << " < " << size << "]" << std::endl;
            close();
            chatServer_->clearSession(sessionId_);
            return;
        }

        memcpy(recvNode_->buffer_, buffer_, bytes_transfer);
        recvNode_->used_ += bytes_transfer;
        recvNode_->buffer_[recvNode_->capacity_] = '\0';
        std::cout << "Recv msg body: " << recvNode_->buffer_ << std::endl;

        // 处理接收数据
        const auto logicNode = std::make_shared<LogicNode>(self, recvNode_);
        ChatLogicSystem::getInstance()->insertMsgNode(logicNode);

        // 继续接收头部数据
        asyncReadHead(HEAD_TOTAL_LEN);
    });
}

void Session::asyncReadFull(const std::uint16_t totalLen,
                            const std::function<void(const boost::system::error_code &, uint16_t)>& callback) {
    memset(buffer_, 0, sizeof(buffer_));
    asyncReadSome(0, totalLen, callback);
}

void Session::asyncReadSome(std::uint16_t readLen, std::uint16_t totalLen,
    const std::function<void(const boost::system::error_code &, uint16_t)>& callback) {
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

void Session::asyncSend() {
    const auto& node = sendNodeQueue_.front();
    auto self = shared_from_this();
    std::cout << "async_write: " << node->used_ << " body: " << node->buffer_ + HEAD_TOTAL_LEN << std::endl;
    boost::asio::async_write(socket_, boost::asio::buffer(node->buffer_, node->used_),
        [self, this](const boost::system::error_code& error, size_t bytes_transfer) {
            if (error) {
                std::cout << "Session: " << sessionId_;
                std::cout << " Handle write failed, error: " << error.what() << std::endl;
                close();
                chatServer_->clearSession(sessionId_);
                return;
            }

            std::lock_guard<std::mutex> lock(sendMtx_);
            sendNodeQueue_.pop();
            if (!sendNodeQueue_.empty()) {
                asyncSend();
            }
        });
}
