//
// Created by Fan on 2026/5/12.
//

#include <json/value.h>
#include <json/reader.h>

#include "ChatLogicSystem.h"

#include "StatusGrpcClient.h"
#include "Session.h"
#include "MysqlMgr.h"
#include "RedisMgr.h"
#include "ConfigMgr.h"
#include "UserMgr.h"
#include "ChatGrpcClient.h"

ChatLogicSystem::~ChatLogicSystem() {
    close();
    cond_.notify_all();
    worker_.join();
}

void ChatLogicSystem::close() {
    stop_.store(true);
}

void ChatLogicSystem::setServerName(const std::string &name) {
    selfServerName_ = name;
}

void ChatLogicSystem::insertMsgNode(const std::shared_ptr<LogicNode> &msg) {
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
    registerHandler(static_cast<uint16_t>(MessageID::ID_CHAT_LOGIN),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return loginHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_USER_SEARCH_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return searchUserHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_ADD_FRIEND_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return addFriendHandle(session, msgId, data);
        });
}

void ChatLogicSystem::registerHandler(uint16_t msgId, const msgHandler& handler) {
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
    std::cout << "Handle msg id is " << node->node_->msgId_ << std::endl;
    if (handlers_.find(node->node_->msgId_) == handlers_.end()) {
        std::cout << "Msg id [" << node->node_->msgId_ << "] handler not found" << std::endl;
        // todo 回复异常响应
        return;
    }
    handlers_[node->node_->msgId_](node->session_, node->node_->msgId_, std::string(node->node_->buffer_));
}

bool ChatLogicSystem::getUserBaseInfo(const std::string &key, int uid, std::shared_ptr<UserInfo> &userInfo) {
    if (std::string info; RedisMgr::getInstance()->get(key, info)) {
        Json::Value root;
        if (Json::Reader reader; !reader.parse(info, root)) {
            std::cout << "Failed to parse JSON data" << std::endl;
            return false;
        }
        userInfo->uid = root["uid"].asInt();
        userInfo->name = root["name"].asString();
        userInfo->password = root["passwd"].asString();
        userInfo->email = root["email"].asString();
    }
    else {
        std::shared_ptr<UserInfo> user = nullptr;
        user = MysqlMgr::getInstance()->getUser(uid);
        if (nullptr == user) {
            std::cout << "Not found user by uid " << uid << std::endl;
            return false;
        }
        Json::Value root;
        root["uid"] = user->uid;
        userInfo->uid = user->uid;
        root["name"] = user->name;
        userInfo->name = user->name;
        root["passwd"] = user->password;
        userInfo->password = user->password;
        root["email"] = user->password;
        userInfo->email = user->email;
    }
    return true;
}

bool ChatLogicSystem::getUserInfoByName(const std::string &name, Json::Value& root) {
    if (std::string info; RedisMgr::getInstance()->get(USER_BASE_INFO_PREFIX + name, info)) {
        if (Json::Reader reader; !reader.parse(info, root)) {
            std::cout << "Failed to parse JSON data" << std::endl;
            return false;
        }
    }
    else {
        std::shared_ptr<UserInfo> user = nullptr;
        user = MysqlMgr::getInstance()->getUser(name);
        if (nullptr == user) {
            std::cout << "Not found user by name " << name << std::endl;
            return false;
        }
        root["uid"] = user->uid;
        root["name"] = user->name;
        root["email"] = user->email;
        const std::string jsonStr = root.toStyledString();
        RedisMgr::getInstance()->set(USER_BASE_INFO_PREFIX + user->name, jsonStr);
    }
    return true;
}

void ChatLogicSystem::loginHandle(const std::shared_ptr<Session>& session, const uint16_t msgId, const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_CHAT_LOGIN_RSP));
    });
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_JSON);
        return;
    }
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
    const auto uid = srcRoot["uid"].asInt();
    std::cout << "user login uid is " << uid << std::endl;
    const auto reply = StatusGrpcClient::getInstance()->Login(uid, srcRoot["token"].asString());
    if (reply.error() != static_cast<int32_t>(ErrorCodes::SUCCESS)
        || reply.token() != srcRoot["token"].asString()) {
        std::cout << "Login token error, expect: " << reply.token() << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::CHAT_LOGIN_TOKEN_ERROR);
        return;
    }

    // 查询用户是否存在
    auto user = std::make_shared<UserInfo>();
    if (!getUserBaseInfo(USER_BASE_INFO_PREFIX + std::to_string(uid), uid, user)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::CHAT_LOGIN_UID_ERROR);
        return;
    }

    root["uid"] = uid;
    root["name"] = user->name;
    root["email"] = user->email;
    root["token"] = reply.token();

    // 获取申请列表
    // 获取好友列表
    // 增加登录数量
    auto serverName = ConfigMgr::getInstance().getValue("ChatServer", "Name");
    auto res = RedisMgr::getInstance()->hGet(LOGIN_COUNT, serverName);
    int count = 0;
    if (res.empty()) {
        count = std::stoi(res);
    }
    ++count;
    RedisMgr::getInstance()->hSet(LOGIN_COUNT, serverName, std::to_string(count));

    // Session 与 uid 绑定
    session->setUserId(uid);
    UserMgr::getInstance()->setUserSession(uid, session);

    // 设置用户登录地址服务名
    RedisMgr::getInstance()->set(USER_IP_PREFIX + std::to_string(uid), serverName);
}

void ChatLogicSystem::searchUserHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
    const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_USER_SEARCH_RSP));
    });
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_JSON);
        return;
    }
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
    Json::Value dataRoot;
    if (!srcRoot["name"].asString().empty()) {
        const auto name = srcRoot["name"].asString();
        getUserInfoByName(name, dataRoot);
        root["data"] = dataRoot;
    }
}

void ChatLogicSystem::addFriendHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
    const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_ADD_FRIEND_RSP));
    });
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_JSON);
        return;
    }
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
    // 保存好友申请记录
    const auto from = srcRoot["fromUid"].asInt();
    const auto to = srcRoot["toUid"].asInt();
    if (!MysqlMgr::getInstance()->addFriendApply(from, to)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::MYSQL_ERROR);
        return;
    }

    // 查找用户是否在线
    std::string toServiceName;
    if (!RedisMgr::getInstance()->get(USER_IP_PREFIX + std::to_string(to), toServiceName)) {
        return; // 用户不在线直接返回，等到上线直接从数据库拉取
    }

    // 同一服务器直接发送申请消息
    if (toServiceName == selfServerName_) {
        if (auto toSession = UserMgr::getInstance()->getSession(to)) {
            Json::Value applyRoot;
            applyRoot["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
            applyRoot["fromUid"] = from;
            applyRoot["applyName"] = srcRoot["applyName"];
            applyRoot["applyEmail"] = srcRoot["applyEmail"];
            toSession->asyncSend(applyRoot.toStyledString(), static_cast<std::uint16_t>(MessageID::ID_NOTIFY_ADD_FRIEND_REQ));
        }
        return;
    }

    // 不同服务器调用 grpc 请求
    AddFriendReq request;
    request.set_from_uid(from);
    request.set_to_uid(to);
    request.set_name(srcRoot["applyName"].asString());
    request.set_email(srcRoot["applyEmail"].asString());
    ChatGrpcClient::getInstance()->NotifyAddFriend(toServiceName, request);
}
