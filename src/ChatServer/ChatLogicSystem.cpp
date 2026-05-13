//
// Created by Fan on 2026/5/12.
//

#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>

#include "ChatLogicSystem.h"

#include <boost/any/detail/placeholder.hpp>

#include "StatusGrpcClient.h"

#include "Session.h"

ChatLogicSystem::~ChatLogicSystem() {
    close();
    cond_.notify_all();
    worker_.join();
}

void ChatLogicSystem::close() {
    stop_.store(true);
}

void ChatLogicSystem::insertMsgNode(std::shared_ptr<LogicNode> msg) {
    std::unique_lock<std::mutex> lock(mutex_);
    msgQueue_.push(msg);
    if (1 == msgQueue_.size()) {
        // 空队列阻塞后通知开始处理，直到队列处理完所有消息
        lock.unlock(); // 通知前解锁，确保其他线程能获取锁取消息
        cond_.notify_one();
    }
}

ChatLogicSystem::ChatLogicSystem() : stop_(false) {
    initHandlers();
    worker_ = std::thread(&ChatLogicSystem::dealMsg, this);
}

void ChatLogicSystem::initHandlers() {
    registerHandler(static_cast<uint16_t>(MessageID::CHAT_LOGIN),
        std::bind(&ChatLogicSystem::loginHandle, this,
        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void ChatLogicSystem::registerHandler(uint16_t msgId, msgHandler handler) {
    if (handlers_.find(msgId) != handlers_.end()) {
        return;
    }
    handlers_.insert({msgId, handler});
}

void ChatLogicSystem::dealMsg() {
    while (true) {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this]() {
            if (stop_.load()) {
                return true;
            }
            return !msgQueue_.empty();
        });
        if (stop_.load()) {
            // 服务器关闭前将已经收到的消息处理完
            while (!msgQueue_.empty()) {
                auto msgNode = msgQueue_.front();
                handleMsgNode(msgNode);
                msgQueue_.pop();
            }
            break;
        }

        auto msgNode = msgQueue_.front();
        handleMsgNode(msgNode);
        msgQueue_.pop();
    }
}

void ChatLogicSystem::handleMsgNode(const std::shared_ptr<LogicNode> &node) {
    std::cout << "Handle msg id is " << node->msgId_;
    if (handlers_.find(node->msgId_) == handlers_.end()) {
        std::cout << "Msg id [" << node->msgId_ << "] handler not found" << std::endl;
        return;
    }
    handlers_[node->msgId_](node->session_, node->msgId_, std::string(node->buffer_));
}

void ChatLogicSystem::loginHandle(std::shared_ptr<Session> session, const uint16_t msgId, const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::CHAT_LOGIN_RSP));
    });
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_JSON);
        return;
    }
    auto uid = srcRoot["uid"].asInt();
    std::cout << "user login uid is " << uid << std::endl;
    auto reply = StatusGrpcClient::getInstance()->Login(uid, srcRoot["token"].asString());
}
