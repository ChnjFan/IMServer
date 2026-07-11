//
// Created by Fan on 2026/5/12.
//

#include <regex>

#include <json/value.h>
#include <json/reader.h>

#include "ChatLogicSystem.h"

#include "StatusGrpcClient.h"
#include "Session.h"
#include "RedisMgr.h"
#include "ConfigMgr.h"
#include "UserMgr.h"
#include "ChatGrpcClient.h"
#include "FriendCache.h"
#include "LogicWorker.h"

#include "db/mysql/MysqlMgr.h"
#include "db/redis/UserInfoCache.h"
#include "common/model/ConversationInfo.h"
#include "common/model/MessageInfo.h"

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

void ChatLogicSystem::notifyOnlineUserMsg(const int uid, const std::string &msg, MessageID msgId,
    const notifyOnlineUserCallback &callback) {
    const auto toServiceName = RedisMgr::getInstance()->hGet(
        USER_ONLINE_INFO_PREFIX + std::to_string(uid), USER_ONLINE_SERVER_NAME);
    if (toServiceName.empty()) {
        return;// 用户不在线直接返回，等到上线直接从数据库拉取
    }

    // 同一服务器直接发送申请消息
    if (toServiceName == selfServerName_) {
        if (const auto toSession = UserMgr::getInstance()->getSession(uid)) {
            toSession->asyncSend(msg, static_cast<std::uint16_t>(msgId));
        }
        return;
    }

    return callback(toServiceName);
}

ChatLogicSystem::ChatLogicSystem() : stop_(false), workerPool_() {
    initHandlers();
    worker_ = std::thread(&ChatLogicSystem::dealMsg, this);
    workerPool_.start();
}

void ChatLogicSystem::initHandlers() {
    registerHandler(static_cast<uint16_t>(MessageID::ID_CHAT_LOGIN),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return loginHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_FIRST_PAGE_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return firstPageInfoHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_GET_FRIEND_LIST_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return searchFriendListHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_USER_SEARCH_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return searchUserHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_GET_USER_FULL_INFO_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return searchUserFullInfoHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_FRIEND_APPLY_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return friendApplyHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_FRIEND_AUTH_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return friendAuthHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_GET_FRIEND_REPLY_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return searchFriendApplyListHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_UPDATE_FRIEND_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return updateFriendHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_UPDATE_USERINFO_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return updateUserInfoHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_CHAT_CONVERSATION_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return conversationCreateHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_CONV_LIST_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return conversationListFetchHandle(session, msgId, data);
        });

    registerHandler(static_cast<uint16_t>(MessageID::ID_CHAT_MSG_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return chatMsgHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_CONV_HISTORY_MSG_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return historyChatMsgFetchHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_CONV_MSG_UPDATE_STATUS_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return msgStatusUpdateHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_HEART_BEAT_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return heartbeatHandle(session, msgId, data);
        });

    registerHandler(static_cast<uint16_t>(MessageID::ID_CHAT_UPLOAD_FILE_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return uploadFileHandle(session, msgId, data);
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
    node->session_->updateLstActiveTime();

    if (handlers_.find(node->node_->msgId_) == handlers_.end()) {
        std::cout << "Msg id [" << node->node_->msgId_ << "] handler not found" << std::endl;
        Json::Value msg;
        msg["error"] = static_cast<int32_t>(ErrorCodes::REQUEST_NOT_FOUND);
        node->session_->asyncSend(msg.toStyledString(), static_cast<uint16_t>(MessageID::ID_CLIENT_COMMON_RSP));
        return;
    }
    try {
        handlers_[node->node_->msgId_](node->session_, node->node_->msgId_, std::string(node->node_->buffer_));
    } catch (...) {
        std::cout << "Handle msg [" << node->node_->msgId_ << "] not found!" << std::endl;
        Json::Value msg;
        msg["error"] = static_cast<int32_t>(ErrorCodes::REQUEST_NOT_FOUND);
        node->session_->asyncSend(msg.toStyledString(), node->node_->msgId_ + 1);
    }
}

void ChatLogicSystem::kickOnlineUser(const int uid) const {
    const std::string serverName = RedisMgr::getInstance()->hGet(
        USER_ONLINE_INFO_PREFIX + std::to_string(uid),USER_ONLINE_SERVER_NAME);
    if (serverName.empty()) {
        return;
    }
    if (selfServerName_ == serverName) {// 用户在本服务器
        if (const auto oldSession = UserMgr::getInstance()->getSession(uid)) {
            oldSession->notifyOffline();
        }
    }
    else {// 用户在其他服务器，通知对端离线
        ChatGrpcClient::getInstance()->NotifyOffline(serverName, uid);
    }
}

void ChatLogicSystem::loginHandle(const std::shared_ptr<Session> &session, const uint16_t msgId,
                                  const std::string &data) const {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_CHAT_LOGIN_RSP));
    });
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
    const auto uid = srcRoot["uid"].asString();
    const int userid = std::stoi(uid);
    std::cout << "user login uid is " << uid << std::endl;
    const auto reply = StatusGrpcClient::getInstance()->Login(userid, srcRoot["token"].asString());
    if (reply.error() != static_cast<int32_t>(ErrorCodes::SUCCESS)
        || reply.token() != srcRoot["token"].asString()) {
        std::cout << "Login token error, expect: " << reply.token() << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::CHAT_LOGIN_TOKEN_ERROR);
        return;
    }

    // 查询用户是否存在，返回基本信息
    UserBaseInfo userInfo;
    userInfo.uid = userid;
    if (!searchUserBaseInfo(userInfo)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::CHAT_LOGIN_UID_ERROR);
        return;
    }

    userInfo.toJson(root);
    root["token"] = reply.token();

    // 服务端踢人逻辑，将其他在线客户端下线
    kickOnlineUser(userid);
    // Session 与 uid 绑定
    session->setUserId(userid);
    UserMgr::getInstance()->setUserSession(userid, session);
    session->updateState(SessionState::ONLINE);
}

int ChatLogicSystem::getApplyFriendCount(const int uid) {
    int count = 0;
    if (FriendCache::getFriendApplyCount(uid, count)) {
        return count;
    }

    if (!MysqlMgr::getInstance()->getFriendApplyCount(uid, count)) {
        return 0;
    }

    if (!FriendCache::updateFriendApplyCount(uid, count)) {
        std::cout << "Friend apply count error" << std::endl;
    }

    return count < 0 ? 0 : count;
}

void ChatLogicSystem::firstPageInfoHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
                                          const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_FIRST_PAGE_RSP));
    });
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
    const auto uid = std::stoi(srcRoot["uid"].asString());
    // 获取未处理的好友申请计数
    root["friend_apply_count"] = getApplyFriendCount(uid);
}

void ChatLogicSystem::searchFriendListHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
                                             const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_GET_FRIEND_LIST_RSP));
    });
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);

    auto sinceTime = srcRoot["since_update_time"].asString();
    if (sinceTime.empty()) {
        sinceTime = "0000-00-00 00:00:00";
    }
    const auto uid = std::stoi(srcRoot["uid"].asString());
    if (uid < 0) {
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }

    const auto friendList = MysqlMgr::getInstance()->selectFriendList(uid, sinceTime);
    if (friendList.empty()) {
        root["error"] = static_cast<int32_t>(ErrorCodes::FRIEND_NOT_EXISTS);
    }

    for (const auto& friendInfo : friendList) {
        Json::Value friendJson;
        friendInfo.toJson(friendJson);
        root["data"].append(friendJson);
    }
}

void ChatLogicSystem::getSearchInfoFromJson(Json::Value &root, UserBaseInfo &userInfo) {
    if (root.isMember("uid")) {
        userInfo.uid = std::stoi(root["uid"].asString());
    }
    if (root.isMember("email")) {
        userInfo.email = root["email"].asString();
    }
    if (root.isMember("name")) {
        userInfo.name = root["name"].asString();
    }
}

bool ChatLogicSystem::searchUserFullInfo(UserBaseInfo &baseInfo, UserProfile& profile) {
    // Redis 缓存直接通过 uid 查询
    if (UserInfoCache::searchUserFullInfo(baseInfo, profile)) {
        return true;
    }

    if (!MysqlMgr::getInstance()->selectUserFullInfo(baseInfo, profile)) {
        std::cout << "Not found user by uid " << baseInfo.uid << std::endl;
        return false;
    }
    // 更新缓存
    Json::Value baseInfoRoot;
    baseInfo.toJson(baseInfoRoot);
    RedisMgr::getInstance()->set(USER_BASE_INFO_PREFIX + std::to_string(baseInfo.uid),
        baseInfoRoot.toStyledString());
    Json::Value profileInfoRoot;
    profile.toJson(profileInfoRoot);
    RedisMgr::getInstance()->set(USER_PROFILE_INFO_PREFIX + std::to_string(baseInfo.uid),
        profileInfoRoot.toStyledString());

    return true;
}

bool ChatLogicSystem::isFriend(const int uid, const int friendId) {
    if (FriendCache::isFriend(uid, friendId)) {
        return true;
    }

    if (!MysqlMgr::getInstance()->isFriendExist(uid, friendId)) {
        std::cout << "Not found user " << uid << " friend by uid " << friendId << std::endl;
        return false;
    }

    // 更新好友关系集合缓存
    if (!FriendCache::updateFriendSet(uid, friendId)) {
        std::cout << "update friend relation failed" << std::endl;
    }

    return true;
}

void ChatLogicSystem::setFriendRelation(const int uid, const int friendId, Json::Value &root) {
    if (friendId == uid || isFriend(uid, friendId)) {
        root["friend_status"] = static_cast<uint8_t>(FriendStatus::FRIEND_PRESENT);
        return;
    }

    root["friend_status"] = static_cast<uint8_t>(FriendStatus::NOT_FRIEND);
}

void ChatLogicSystem::searchUserFullInfoHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
                                               const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_GET_USER_FULL_INFO_RSP));
    });
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);

    UserBaseInfo baseInfo;
    UserProfile profile;
    getSearchInfoFromJson(srcRoot, baseInfo);
    if (!searchUserFullInfo(baseInfo, profile)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    baseInfo.toJson(root);
    profile.toJson(root);

    // 设置好友关系
    int from = -1;
    if (srcRoot.isMember("from")) {
        from = std::stoi(srcRoot["from"].asString());
    }
    setFriendRelation(from, baseInfo.uid, root);

    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
}


std::string ChatLogicSystem::getSearchKey(UserBaseInfo &userInfo) {
    if (userInfo.email.has_value()) {
        return userInfo.email.value();
    }
    if (userInfo.name.has_value()) {
        return userInfo.name.value();
    }
    return "";
}

bool ChatLogicSystem::searchUserBaseInfo(UserBaseInfo& userInfo) {
    if (UserInfoCache::searchUserBaseInfo(userInfo)) {
        return true;
    }
    // 缓存没有映射关系，只能去数据库查询
    if (!MysqlMgr::getInstance()->selectUserBaseInfo(userInfo) || userInfo.uid < 0) {
        std::cout << "Not found user [uid: " << userInfo.uid
            << " email: " << userInfo.email.value()
            << " name: " << userInfo.name.value() << "]" << std::endl;
        return false;
    }
    // 更新缓存
    if (!UserInfoCache::updateBaseInfo(userInfo)) {
        std::cout << "Failed to update user info" << std::endl;
    }
    if (!UserInfoCache::updateUidMap(userInfo)) {
        std::cout << "Failed to update uid map" << std::endl;
    }
    return true;
}

void ChatLogicSystem::searchUserHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
                                       const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_USER_SEARCH_RSP));
    });
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }

    UserBaseInfo searchInfo;
    searchInfo.fromJson(srcRoot);
    if (!searchUserBaseInfo(searchInfo)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }

    // ReSharper disable once CppDFAConstantConditions
    if (searchInfo.uid < 0) {
        root["error"] = static_cast<int32_t>(ErrorCodes::USER_NOT_EXISTS);
        return;
    }

     searchInfo.toJson(root);
}

void ChatLogicSystem::searchFriendApplyListHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
                                                  const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_GET_FRIEND_REPLY_RSP));
    });
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
    const auto uid = std::stoi(srcRoot["uid"].asString());
    auto sinceTime = srcRoot["since_update_time"].asString();
    if (sinceTime.empty()) {
        sinceTime = "0000-00-00 00:00:00";
    }
    const std::vector<FriendApply> searchResult = MysqlMgr::getInstance()->selectFriendApplyList(uid, sinceTime);
    if (searchResult.empty()) {
        root["error"] = static_cast<int32_t>(ErrorCodes::FRIEND_APPLY_NOT_EXISTS);
        return;
    }

    for (auto& searchInfo : searchResult) {
        Json::Value info;
        searchInfo.toJson(info);
        root["data"].append(info);
    }

}

bool ChatLogicSystem::checkFriendRelation(const int uid, const int friendId) {
    if (FriendCache::isFriend(uid, friendId)) {
        return true;
    }

    if (!MysqlMgr::getInstance()->isFriendExist(uid, friendId)) {
        return false;
    }

    // 更新好友关系
    FriendCache::updateFriendSet(uid, friendId);

    return true;
}

bool ChatLogicSystem::checkFriendApplyInvalid(const int uid, const int friendId) {
    // 检查是否是好友
    if (checkFriendRelation(uid, friendId)) {
        return true;
    }

    // 检查对方已经是好友，直接恢复好友关系，恢复失败走普通好友申请
    if (checkFriendRelation(friendId, uid)) {
        FriendInfo info;
        info.friendId = friendId;
        info.status = static_cast<int8_t>(FriendStatus::FRIEND_PRESENT);
        if (!MysqlMgr::getInstance()->updateFriendRelation(uid, info)) {
            return false;
        }
        return true;
    }

    // 检查是否已经存在提交记录
    if (MysqlMgr::getInstance()->checkFriendApplyExist(uid, friendId)) {
        return true;
    }

    return false;
}

void ChatLogicSystem::friendApplyHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
                                        const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_FRIEND_APPLY_RSP));
    });
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
    // 申请参数校验：是否好友、是否已经有存在的申请防止重复提交
    const auto from = std::stoi(srcRoot["uid"].asString());
    const auto to = std::stoi(srcRoot["friend_id"].asString());
    if (checkFriendApplyInvalid(from, to)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::USER_IS_FRIEND_RELATION);
        return;
    }

    // 保存申请记录
    std::string msg;
    if (srcRoot.isMember("message")) {
        msg = srcRoot["message"].asString();
    }
    if (!MysqlMgr::getInstance()->updateFriendApply(from, to, 0, msg)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::MYSQL_ERROR);
        return;
    }
    FriendCache::clearFriendApplyCount(to);

    // 通知在线用户
    notifyOnlineUserMsg(to, data, MessageID::ID_NOTIFY_FRIEND_APPLY,
            [from, to, &data](const std::string& serverName) {
        // 不同服务器调用 grpc 请求
        ChatServiceReq request;
        request.set_from_uid(from);
        request.set_to_uid(to);
        request.set_json(data);
        ChatGrpcClient::getInstance()->NotifyAddFriend(serverName, request);
    });
}

void ChatLogicSystem::friendAuthHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
    const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_FRIEND_AUTH_RSP));
    });
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);

    FriendApply applyInfo;
    applyInfo.fromJson(srcRoot);
    // 认证方发送的消息中，friend_id 是申请的发起人，uid 是认证方，所以更新申请时要调换 uid 和 friend_id
    std::swap(applyInfo.uid, applyInfo.friendId);
    if (applyInfo.friendId < 0 || applyInfo.uid < 0) {
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }

    if (!srcRoot.isMember("result")) {
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    if (const auto result = srcRoot["result"].asInt(); result != 1) {
        // 已拒绝好友，直接更新数据库删除缓存
        MysqlMgr::getInstance()->updateFriendApply(applyInfo.uid, applyInfo.friendId,
            static_cast<int>(FriendApplyStatus::REJECT));
        FriendCache::clearFriendApplyCount(applyInfo.friendId);
        return;
    }

    // 更新好友申请状态，并同步创建双向好友关系
    if (!MysqlMgr::getInstance()->createFriendRelation(applyInfo)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::MYSQL_ERROR);
        return;
    }
    // 清除被申请人的未读计数
    FriendCache::clearFriendApplyCount(applyInfo.friendId);

    // 推送好友请求信息
    notifyOnlineUserMsg(applyInfo.uid, data, MessageID::ID_NOTIFY_FRIEND_AUTH,
        [&applyInfo, &data, &root](const std::string& serverName) {
        ChatServiceReq request;
        request.set_from_uid(applyInfo.friendId);
        request.set_to_uid(applyInfo.uid);
        request.set_json(data);
        const auto resp = ChatGrpcClient::getInstance()->NotifyAuthFriend(serverName, request);
        root["error"] = resp.error();
    });
}

void ChatLogicSystem::updateFriendHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
    const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_UPDATE_FRIEND_RSP));
    });
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);

    FriendInfo info;
    info.fromJson(srcRoot);
    const auto uid = std::stoi(srcRoot["uid"].asString());
    if (!MysqlMgr::getInstance()->updateFriendRelation(uid, info)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::MYSQL_ERROR);
    }

    if (info.status == static_cast<int8_t>(FriendStatus::FRIEND_DELETED)) {
        // 单方面删除好友关系
        FriendCache::deleteFriendSet(uid, info.friendId);
    }
}

void ChatLogicSystem::updateUserInfoHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
                                           const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_UPDATE_USERINFO_RSP));
    });
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);

    // 暂时只能支持修改一个字段
    const auto uid = std::stoi(srcRoot["uid"].asString());
    UserBaseInfo oldInfo;
    oldInfo.uid = uid;
    if (!searchUserBaseInfo(oldInfo)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::USER_NOT_EXISTS);
        return;
    }

    UserBaseInfo info;
    info.fromJson(srcRoot);
    UserProfile profile;
    profile.fromJson(srcRoot);
    if (!MysqlMgr::getInstance()->updateUserBaseInfo(info)
        && !MysqlMgr::getInstance()->updateUserProfileInfo(profile)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::MYSQL_ERROR);
        return;
    }

    if (!RedisMgr::getInstance()->del(USER_BASE_INFO_PREFIX + std::to_string(uid))
        || !RedisMgr::getInstance()->del(USER_PROFILE_INFO_PREFIX + std::to_string(uid))
        || !RedisMgr::getInstance()->del(UID_INDEX_MAP_PREFIX + oldInfo.email.value())
        || !RedisMgr::getInstance()->del(UID_INDEX_MAP_PREFIX + oldInfo.name.value())) {
        std::cout << "Failed to delete user info, waiting to add delay task" << std::endl;
    }
}

// todo 后续优化性能
bool ChatLogicSystem::isPrivateChat(const int uid) {
    UserProfile profile;
    if (UserInfoCache::getUserProfile(uid, profile) && profile.privacyChat >= 0) {
        return profile.privacyChat == 1;
    }

    if (!MysqlMgr::getInstance()->selectUserProfileInfo(uid, profile)) {
        return false;
    }

    return profile.privacyChat == 1;
}

bool ChatLogicSystem::checkConversationValid(const int uid, const int other) {
    // 自己也可以跟自己建立会话
    if (uid == other) {
        return true;
    }

    if (isFriend(uid, other) || isPrivateChat(other)) {
        return true;
    }

    return false;
}

void ChatLogicSystem::conversationCreateHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
                                               const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_CHAT_CONVERSATION_RSP));
    });
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }

    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);

    ConversationInfo convInfo;
    convInfo.fromJson(srcRoot);
    if (!checkConversationValid(convInfo.uid, convInfo.friendId)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::CONV_CREATE_NO_PERMISSION);
        return;
    }

    // 创建单聊会话生成会话 ID
    convInfo.convType = static_cast<int8_t>(ConvType::PRIVATE_CHAT);
    convInfo.generateConvId();

    std::string result;
    if (!MysqlMgr::getInstance()->createConversation(convInfo, result)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::MYSQL_ERROR);
        return;
    }

    // 入库成功后生成回复消息内容
    convInfo.status = 0;
    convInfo.isTop = 0;
    convInfo.isMute = 0;
    convInfo.lastMsgContent = "";
    convInfo.lastTime = "";
    convInfo.updateTime = result;
    convInfo.toJson(root);
}

void ChatLogicSystem::getConversationTitleInfo(const ConversationInfo& convInfo, Json::Value &root) {
    const auto otherUid = convInfo.getOtherUid();
    if (otherUid < 0) {
        std::cout << "getConversationTitleInfo get other uid error" << std::endl;
        return;
    }
    UserBaseInfo userBaseInfo;
    userBaseInfo.uid = otherUid;
    if (!searchUserBaseInfo(userBaseInfo)) {
        return;
    }
    if (userBaseInfo.name.has_value()) {
        root["title"] = userBaseInfo.name.value();
    }
    if (userBaseInfo.avatarUrl.has_value()) {
        root["avatar_url"] = userBaseInfo.avatarUrl.value();
    }
}

void ChatLogicSystem::conversationListFetchHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
                                                  const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_CONV_LIST_RSP));
    });
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
    const auto uid = std::stoi(srcRoot["uid"].asString());
    auto sinceTime = srcRoot["since_update_time"].asString();
    if (sinceTime.empty()) {
        sinceTime = "0000-00-00 00:00:00";
    }
    const std::vector<ConversationInfo> searchResult = MysqlMgr::getInstance()->selectConversationList(uid, sinceTime);
    if (searchResult.empty()) {
        root["error"] = static_cast<int32_t>(ErrorCodes::FRIEND_APPLY_NOT_EXISTS);
        return;
    }

    for (auto& searchInfo : searchResult) {
        Json::Value info;
        searchInfo.toJson(info);
        searchInfo.uid = uid;
        getConversationTitleInfo(searchInfo, info);
        root["data"].append(info);
    }
}

void ChatLogicSystem::chatMsgHandle(const std::shared_ptr<Session> &session, uint16_t msgId, const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, &srcRoot, session, this]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_CHAT_MSG_RSP));
    });
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
    root["conv_id"] = srcRoot["conv_id"];

    MessageInfo info;
    info.fromJson(srcRoot);
    info.status = static_cast<uint8_t>(MessageStatus::SENDING);
    int serverId = -1;
    if (!MysqlMgr::getInstance()->createMessage(info, serverId)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::MYSQL_ERROR);
        return;
    }
    root["server_id"] = serverId;

    notifyOnlineUserMsg(info.toUid, data, MessageID::ID_NOTIFY_CHAT_MSG,
            [serverId, &root, &info, &data](const std::string& serverName) {
        ChatServiceReq request;
        request.set_from_uid(info.fromUid);
        request.set_to_uid(info.toUid);
        request.set_json(data);
        const auto reply = ChatGrpcClient::getInstance()->SendChatMsg(serverName, request);
        if (reply.error() == static_cast<int32_t>(ErrorCodes::SUCCESS)
            && !MysqlMgr::getInstance()->updateMessageStatus(serverId, MessageStatus::IS_SEND)) {
            root["error"] = static_cast<int32_t>(ErrorCodes::MYSQL_ERROR);
        }
    });
}

void ChatLogicSystem::historyChatMsgFetchHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
    const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_CONV_HISTORY_MSG_RSP));
    });
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);

    const auto convId = srcRoot["conv_id"].asString();
    const auto sinceMsgId = srcRoot["since_msg_id"].asInt();
    const auto limit = srcRoot["limit"].asInt();
    const std::vector<MessageInfo> searchResult = MysqlMgr::getInstance()->selectMessageList(convId, sinceMsgId, limit);
    if (searchResult.empty()) {
        root["has_more"] = 0;
        return;
    }

    for (auto& searchInfo : searchResult) {
        Json::Value info;
        searchInfo.toJson(info);
        root["data"].append(info);
    }

    root["has_more"] = searchResult.size() < limit ? 0 : 1;
}

void ChatLogicSystem::msgStatusUpdateHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
    const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_CONV_MSG_UPDATE_STATUS_RSP));
    });
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);

    MessageStatusInfo info;
    info.fromJson(srcRoot);
    if (!MysqlMgr::getInstance()->updateConvMessagesStatus(info)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::MYSQL_ERROR);
        return;
    }
}

void ChatLogicSystem::heartbeatHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
                                      const std::string &data) {
    Json::Value root;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_HEART_BEAT_RSP));
    });

    session->updateLstActiveTime();
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
}

/**
 * @brief 上传文件
 *
 * @note 再工作线程中处理，避免阻塞
 */
void ChatLogicSystem::uploadFileHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
    const std::string &data) {
    // 发给工作线程处理，不要占用 IO 线程
    const auto worker = std::make_shared<LogicWorker>(session, msgId, data);
    worker->init();
    workerPool_.addTask(worker);
}


