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
#include "LogicWorker.h"
#include "UserInfo.h"
#include "FriendRelation.h"

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
    const notifyDiffServerOnlineUserCallback &callback) {
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
    registerHandler(static_cast<uint16_t>(MessageID::ID_UPDATE_USERINFO_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return updateUserInfoHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_CHAT_MSG_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return chatMsgHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_CHAT_CONVERSATION_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return conversationCreateHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_CHAT_UPLOAD_FILE_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return uploadFileHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_HEART_BEAT_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return heartbeatHandle(session, msgId, data);
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

bool ChatLogicSystem::getConversationList(int uid, ConversationList &convList) {
    std::vector<std::string> convIds(10);
    std::vector<std::string> keys = {
        "conv_type", "to_uid", "unread_count", "last_msg", "last_time", "is_top", "is_mute"
    };
    if (RedisMgr::getInstance()->zRevrange(
        CHAT_CONVER_PREFIX + std::to_string(uid), convIds, 0, 10)) {
        for (auto &convId : convIds) {
            auto info = RedisMgr::getInstance()->hGetAll(CHAT_CONVER_INFO_PREFIX + convId, keys);
            if (info.empty()) {
                continue;
            }
            auto convInfo = std::make_shared<ConversationInfo>();
            convInfo->conv_id = convId;
            convInfo->conv_type = std::stoi(info["conv_type"]);
            convInfo->to_uid = std::stoi(info["to_uid"]);
            convInfo->unread_count = std::stoi(info["unread_count"]);
            convInfo->last_msg_id = std::stoi(info["last_msg_id"]);
            convInfo->last_msg = info["last_msg"];
            convInfo->last_time = info["last_time"];
            convInfo->is_top = std::stoi(info["is_top"]);
            convInfo->is_mute = std::stoi(info["is_mute"]);
            convList.push_back(convInfo);
        }
        if (!convList.empty()) {
            return true;
        }
    }

    if (!MysqlMgr::getInstance()->getConversation(uid, convList)) {
        std::cout << "Not found conversation by uid " << uid << std::endl;
        return false;
    }

    // 更新 Redis
    for (const auto &conv : convList) {
        std::unordered_map<std::string, std::string> convInfo;
        convInfo["conv_id"] = conv->conv_id;
        convInfo["conv_type"] = std::to_string(conv->conv_type);
        convInfo["to_uid"] = std::to_string(conv->to_uid);
        convInfo["unread_count"] = std::to_string(conv->unread_count);
        convInfo["last_msg_id"] = std::to_string(conv->last_msg_id);
        if (!conv->last_msg.empty()) {
            convInfo["last_msg"] = conv->last_msg;
        }
        convInfo["last_time"] = conv->last_time;
        convInfo["is_top"] = std::to_string(conv->is_top);
        convInfo["is_mute"] = std::to_string(conv->is_mute);

        if (!RedisMgr::getInstance()->hSet(CHAT_CONVER_INFO_PREFIX + conv->conv_id, convInfo)) {
            return false;
        }
    }

    return true;
}

void ChatLogicSystem::addHistoryMessage(Json::Value &root, ChatMsgStatus status) {
    MessageInfo message;
    message.conv_id = root["conv_id"].asString();
    message.status = static_cast<uint8_t>(status);
    message.msg_id = root["msg_id"].asInt();
    message.msg_type = root["msg_type"].asInt();
    message.content = root["content"].asString();
    message.sender_uid = root["from_uid"].asInt();
    message.receiver_uid = root["to_uid"].asInt();

    // 更新数据库，事务同时更新会话表和消息表
    if (!MysqlMgr::getInstance()->addHistoryMessage(message)) {
        return;
    }
    // 更新会话
    const auto curTimeStamp = get_current_ms();
    if (!RedisMgr::getInstance()->zSet(
        CHAT_CONVER_PREFIX + std::to_string(message.sender_uid), curTimeStamp, message.conv_id)) {
        return;
    }

    std::unordered_map<std::string, std::string> convInfo;
    convInfo["to_uid"] = std::to_string(message.receiver_uid);
    convInfo["last_time"] = ms_to_datetime(curTimeStamp);
    convInfo["last_msg_id"] = std::to_string(message.msg_id);
    convInfo["last_msg"] = message.content;
    if (!RedisMgr::getInstance()->hSet(CHAT_CONVER_INFO_PREFIX + message.conv_id, convInfo)) {
        std::cout << "Redis set conver info error" << std::endl;
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

void ChatLogicSystem::loginHandle(const std::shared_ptr<Session>& session, const uint16_t msgId, const std::string &data) {
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
    int userid = std::stoi(uid);
    std::cout << "user login uid is " << uid << std::endl;
    const auto reply = StatusGrpcClient::getInstance()->Login(userid, srcRoot["token"].asString());
    if (reply.error() != static_cast<int32_t>(ErrorCodes::SUCCESS)
        || reply.token() != srcRoot["token"].asString()) {
        std::cout << "Login token error, expect: " << reply.token() << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::CHAT_LOGIN_TOKEN_ERROR);
        return;
    }

    // 查询用户是否存在
    UserInfo userInfo;
    if (!searchUserInfoByUid(userid, userInfo)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::CHAT_LOGIN_UID_ERROR);
        return;
    }

    root["uid"] = uid;
    root["name"] = userInfo.name;
    root["email"] = userInfo.email;
    root["avatar_url"] = userInfo.avatarUrl;
    root["token"] = reply.token();

    // 服务端踢人逻辑，将其他在线客户端下线
    kickOnlineUser(userid);
    // Session 与 uid 绑定
    session->setUserId(userid);
    UserMgr::getInstance()->setUserSession(userid, session);
    session->updateState(SessionState::ONLINE);
}

int ChatLogicSystem::getApplyFriendCount(const int uid) {
    const auto count = RedisMgr::getInstance()->hGet(USER_COUNTER_PREFIX + std::to_string(uid),
        USER_FRIEND_REPLY_COUNT);
    if (!count.empty()) {
        return std::stoi(count);
    }

    int countNum = -1;
    if (!MysqlMgr::getInstance()->getFriendApplyCount(uid, countNum)) {
        return 0;
    }

    if (!RedisMgr::getInstance()->hSet(USER_COUNTER_PREFIX + std::to_string(uid),
        USER_FRIEND_REPLY_COUNT, std::to_string(countNum))) {
        std::cout << "Redis set friend_reply count error" << std::endl;
    }
    return countNum >= 0 ? countNum : 0;
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

bool ChatLogicSystem::getFriendList(int uid, int sinceId, std::vector<FriendInfo> &friendList) {
    return MysqlMgr::getInstance()->getFriendList(uid, sinceId, friendList);
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

    const auto sinceId = srcRoot["since_id"].asInt();
    const auto uid = std::stoi(srcRoot["uid"].asString());
    std::vector<FriendInfo> friendList;
    if (!getFriendList(uid, sinceId, friendList) || friendList.empty()) {
        root["error"] = static_cast<int32_t>(ErrorCodes::FRIEND_NOT_EXISTS);
    }

    for (const auto& friendInfo : friendList) {
        Json::Value friendJson;
        // 列表只发送用户名、头像和备注，其他详细信息在好友用户详细页时获取
        friendJson["id"] = friendInfo.relation.id;
        friendJson["friend_id"] = std::to_string(friendInfo.uid);
        friendJson["name"] = friendInfo.userInfo.baseInfo.name;
        friendJson["alias"] = friendInfo.relation.alias;
        friendJson["status"] = friendInfo.relation.status;
        friendJson["avatar_url"] = friendInfo.userInfo.baseInfo.avatarUrl;
        root["data"].append(friendJson);
    }
}

void ChatLogicSystem::getSearchInfoFromJson(Json::Value &root, UserInfo &userInfo) {
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

std::string ChatLogicSystem::getSearchKey(UserInfo &userInfo) {
    if (!userInfo.email.empty()) {
        return userInfo.email;
    }
    if (!userInfo.name.empty()) {
        return userInfo.name;
    }
    return "";
}

bool ChatLogicSystem::searchUserInfoByUid(const int uid, UserInfo &userInfo) {
    if (uid < 0) {
        return false;
    }

    userInfo.uid = uid;
    // Redis 缓存直接通过 uid 查询
    if (std::string info; RedisMgr::getInstance()->
            get(USER_BASE_INFO_PREFIX + std::to_string(userInfo.uid), info)) {
        Json::Value root;
        if (Json::Reader reader; !reader.parse(info, root)) {
            std::cout << "Failed to parse JSON data" << std::endl;
            return false;
        }
        userInfo.fromJson(root);
        return true;
    }

    if (!MysqlMgr::getInstance()->getUserInfo(userInfo)) {
        std::cout << "Not found user by uid " << userInfo.uid << std::endl;
        return false;
    }
    // 更新缓存
    Json::Value root;
    userInfo.toJson(root);
    RedisMgr::getInstance()->set(USER_BASE_INFO_PREFIX + std::to_string(userInfo.uid), root.toStyledString());
    return true;
}

bool ChatLogicSystem::searchUserInfo(UserInfo& userInfo) {
    if (userInfo.uid >= 0) {    // 直接使用 uid 查询
        return searchUserInfoByUid(userInfo.uid, userInfo);
    }
    // 使用其他方式查询，要先查询用户 UID
    const std::string key = UID_INDEX_MAP_PREFIX + getSearchKey(userInfo);
    if (std::string uid; RedisMgr::getInstance()->get(key, uid)) {
        userInfo.uid = std::stoi(uid);
        return searchUserInfoByUid(userInfo.uid, userInfo);
    }
    // 缓存没有映射关系，只能去数据库查询
    if (!MysqlMgr::getInstance()->getUserInfo(userInfo) || userInfo.uid < 0) {
        std::cout << "Not found user [uid: " << userInfo.uid
            << " email: " << userInfo.email
            << " name: " << userInfo.name << "]" << std::endl;
        return false;
    }
    // 更新缓存
    Json::Value root;
    userInfo.toJson(root);
    RedisMgr::getInstance()->set(USER_BASE_INFO_PREFIX + std::to_string(userInfo.uid), root.toStyledString());
    RedisMgr::getInstance()->set(UID_INDEX_MAP_PREFIX + userInfo.email, std::to_string(userInfo.uid));
    RedisMgr::getInstance()->set(UID_INDEX_MAP_PREFIX + userInfo.name, std::to_string(userInfo.uid));
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

    UserInfo searchInfo;
    getSearchInfoFromJson(srcRoot, searchInfo);
    if (!searchUserInfo(searchInfo)) {
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

bool ChatLogicSystem::searchUserFullInfoInRedis(UserFullInfo &userFullInfo) {
    std::string info;
    if (!RedisMgr::getInstance()->get(USER_BASE_INFO_PREFIX + std::to_string(userFullInfo.baseInfo.uid), info)) {
        return false;
    }
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(info, root)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        return false;
    }
    userFullInfo.baseInfo.fromJson(root);
    if (!RedisMgr::getInstance()->get(USER_PROFILE_INFO_PREFIX + std::to_string(userFullInfo.baseInfo.uid), info)) {
        return false;
    }
    if (!reader.parse(info, root)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        return false;
    }
    userFullInfo.profileInfo.fromJson(root);
    return true;
}

bool ChatLogicSystem::searchUserFullInfoByUid(const int uid, UserFullInfo &userFullInfo) {
    if (uid < 0) {
        return false;
    }
    userFullInfo.baseInfo.uid = uid;
    // Redis 缓存直接通过 uid 查询
    if (searchUserFullInfoInRedis(userFullInfo)) {
        return true;
    }

    if (!MysqlMgr::getInstance()->getUserFullInfo(userFullInfo)) {
        std::cout << "Not found user by uid " << userFullInfo.baseInfo.uid << std::endl;
        return false;
    }
    // 更新缓存
    Json::Value baseInfoRoot;
    userFullInfo.baseInfo.toJson(baseInfoRoot);
    RedisMgr::getInstance()->set(USER_BASE_INFO_PREFIX + std::to_string(userFullInfo.baseInfo.uid),
        baseInfoRoot.toStyledString());
    Json::Value profileInfoRoot;
    userFullInfo.profileInfo.toJson(profileInfoRoot);
    RedisMgr::getInstance()->set(USER_PROFILE_INFO_PREFIX + std::to_string(userFullInfo.baseInfo.uid),
        profileInfoRoot.toStyledString());

    return true;
}

bool ChatLogicSystem::getFriendRelationFromRedis(const int from, const int uid, FriendRelation &fr) {
    if (!RedisMgr::getInstance()->sIsMember(FRIEND_SET_PREFIX + std::to_string(from), std::to_string(uid))) {
        return false;   // 没有找到好友关系
    }
    const std::string info = RedisMgr::getInstance()->hGet(FRIEND_RELATION_INFO_PREFIX + std::to_string(from),
            std::to_string(uid));
    if (info.empty()) {
        return false;
    }

    Json::Value root;
    if (Json::Reader reader; reader.parse(info, root)) {
        fr.fromJson(root);
    }
    return true;
}

bool ChatLogicSystem::getFriendRelation(const int from, const int uid, FriendRelation &fr) {
    if (getFriendRelationFromRedis(from, uid, fr)) {
        return true;
    }

    if (!MysqlMgr::getInstance()->getFriendRelation(from, uid, fr)) {
        std::cout << "Not found user " << from << " friend by uid " << uid << std::endl;
        return false;
    }

    // 更新缓存
    Json::Value relationInfoRoot;
    fr.toJson(relationInfoRoot);
    if (!RedisMgr::getInstance()->sAdd(FRIEND_SET_PREFIX + std::to_string(from), std::to_string(uid))) {
        std::cout << "sAdd friend relation failed" << std::endl;
    }
    if (!RedisMgr::getInstance()->hSet(FRIEND_RELATION_INFO_PREFIX + std::to_string(from), std::to_string(uid),
        relationInfoRoot.toStyledString())) {
        std::cout << "hSet friend relation info failed" << std::endl;
    }
    return true;
}

void ChatLogicSystem::setFriendRelation(const int from, const int uid, Json::Value &root) {
    if (from == uid) {
        root["friend_status"] = static_cast<uint8_t>(FriendStatus::FRIEND_PRESENT);
        return;
    }

    if (FriendRelation friendRelation; getFriendRelation(from, uid, friendRelation)) {
        root["friend_status"] = friendRelation.status;
        return;
    }

    // todo 在好友关系列表查询
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

    UserFullInfo searchInfo;
    getSearchInfoFromJson(srcRoot, searchInfo.baseInfo);
    if (!searchUserFullInfoByUid(searchInfo.baseInfo.uid, searchInfo)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    searchInfo.toJson(root);

    // 设置好友关系
    int from = -1;
    if (srcRoot.isMember("from")) {
        from = std::stoi(srcRoot["from"].asString());
    }
    setFriendRelation(from, searchInfo.baseInfo.uid, root);

    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
}

bool ChatLogicSystem::getFriendApplyList(int uid, int sinceId, std::vector<FriendApplyInfo> &applyInfoList) {
    return MysqlMgr::getInstance()->getFriendApplyList(uid, sinceId, applyInfoList);
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
    const auto sinceId = srcRoot["since_id"].asInt();
    std::vector<FriendApplyInfo> searchList;
    if (!getFriendApplyList(uid, sinceId, searchList) || searchList.empty()) {
        root["error"] = static_cast<int32_t>(ErrorCodes::FRIEND_APPLY_NOT_EXISTS);
        return;
    }

    for (auto& searchInfo : searchList) {
        Json::Value info;
        searchInfo.toJson(info);
        root["data"].append(info);
    }

}

bool ChatLogicSystem::checkFriendRelation(const int uid, const int friendId) {
    if (RedisMgr::getInstance()->sIsMember(FRIEND_SET_PREFIX + std::to_string(uid), std::to_string(friendId))) {
        return true;
    }
    if (MysqlMgr::getInstance()->checkFriendRelation(uid, friendId)) {
        return true;
    }
    return false;
}

bool ChatLogicSystem::checkFriendApply(const int uid, const int friendId) {
    if (MysqlMgr::getInstance()->checkFriendApply(uid, friendId)
        || MysqlMgr::getInstance()->checkFriendApply(friendId, uid)) {
        return true;
    }
    return false;
}

bool ChatLogicSystem::checkFriendApplyInvalid(const int uid, const int friendId) {
    // 检查是否是好友
    if (checkFriendRelation(uid, friendId)) {
        return true;
    }

    // 检查是否存在提交记录
    if (checkFriendApply(uid, friendId)) {
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
    if (!MysqlMgr::getInstance()->updateFriendApply(from, to, 0, srcRoot["msg"].asString())) {
        root["error"] = static_cast<int32_t>(ErrorCodes::MYSQL_ERROR);
        return;
    }

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

    FriendInfo friendInfo;
    friendInfo.fromJson(srcRoot);
    if (friendInfo.friendId < 0 || friendInfo.uid < 0) {
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    const auto result = srcRoot["result"].asInt();
    if (result != 1) {
        // 已拒绝好友，直接更新数据库删除缓存
        FriendApplyInfo applyInfo;
        applyInfo.fromJson(srcRoot);
        MysqlMgr::getInstance()->updateFriendApply(applyInfo.friendId, applyInfo.uid,
            static_cast<int>(FriendApplyStatus::REJECT), "");
        RedisMgr::getInstance()->hDel(USER_COUNTER_PREFIX + std::to_string(applyInfo.uid),
            USER_FRIEND_REPLY_COUNT);
        return;
    }

    // 更新好友申请状态，并同步创建双向好友关系
    if (!MysqlMgr::getInstance()->createFriendRelation(friendInfo)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::MYSQL_ERROR);
        return;
    }
    // 清除缓存
    RedisMgr::getInstance()->hDel(USER_COUNTER_PREFIX + std::to_string(friendInfo.uid),
            USER_FRIEND_REPLY_COUNT);

    // 推送好友请求信息
    notifyOnlineUserMsg(friendInfo.friendId, data, MessageID::ID_NOTIFY_FRIEND_AUTH,
        [&friendInfo, &data, &root](const std::string& serverName) {
        ChatServiceReq request;
        request.set_from_uid(friendInfo.uid);
        request.set_to_uid(friendInfo.friendId);
        request.set_json(data);
        const auto resp = ChatGrpcClient::getInstance()->NotifyAuthFriend(serverName, request);
        root["error"] = resp.error();
    });
}

PartsList ChatLogicSystem::getUpdateUserInfoPart(const Json::Value &root) {
    std::vector<std::string> keys = root.getMemberNames();
    PartsList results;
    // 剔除错误码
    std::unordered_set<std::string> removes{"error"};
    std::unordered_set<std::string> stringPart{"email", "name", "avatar_url", "phone", "birthday",
        "region", "signature", "self_intro"};
    std::unordered_set<std::string> numberPart{"gender"};

    for (const auto& key : keys) {
        if (removes.find(key) != removes.end()) {
            continue;
        }
        else if (stringPart.find(key) != stringPart.end()) {
            results["string"].push_back(key);
        }
        else if (numberPart.find(key) != numberPart.end()) {
            results["number"].push_back(key);
        }
    }
    return results;
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

    const auto parts = getUpdateUserInfoPart(srcRoot);
    if (parts.empty()) {
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }

    if (!srcRoot.isMember("uid")) {
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    const auto uid = std::stoi(srcRoot["uid"].asString());
    UserInfo oldInfo;
    if (!searchUserInfoByUid(uid, oldInfo)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::USER_NOT_EXISTS);
        return;
    }

    if (!MysqlMgr::getInstance()->updateUserInfo(srcRoot, parts)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::MYSQL_ERROR);
        return;
    }

    if (!RedisMgr::getInstance()->del(USER_BASE_INFO_PREFIX + std::to_string(uid))
        || !RedisMgr::getInstance()->del(USER_PROFILE_INFO_PREFIX + std::to_string(uid))
        || !RedisMgr::getInstance()->del(UID_INDEX_MAP_PREFIX + oldInfo.email)
        || !RedisMgr::getInstance()->del(UID_INDEX_MAP_PREFIX + oldInfo.name)) {
        std::cout << "Failed to delete user info, waiting to add delay task" << std::endl;
    }
}

/**
 * @brief 会话消息处理函数
 * @param session
 * @param msgId
 * @param data
 *
 * @note
 * 消息格式：
 *     'from_uid': UserSession().uid,
 *     'to_uid': toUid,
 *     'conv_id': c2c_,
 *     'msg_type': 1/2,
 *     'content': text/{'filename': name, 'size': 8.2MB},
 *     'msg_id': localId,
 */
void ChatLogicSystem::chatMsgHandle(const std::shared_ptr<Session> &session, uint16_t msgId, const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    auto status = ChatMsgStatus::SENDING;
    Defer defer([&root, &srcRoot, &status, session, this]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_CHAT_MSG_RSP));
        addHistoryMessage(srcRoot, status);
    });
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
    root["msg_id"] = srcRoot["msg_id"];
    root["conv_id"] = srcRoot["conv_id"];

    const auto from = srcRoot["from_uid"].asInt();
    const auto to = srcRoot["to_uid"].asInt();
    // 检查是否在线
    auto serviceName = RedisMgr::getInstance()->hGet(
        USER_ONLINE_INFO_PREFIX + std::to_string(to), USER_ONLINE_SERVER_NAME);
    if (serviceName.empty()) {
        return;
    }

    if (serviceName == selfServerName_) {
        const auto toSession = UserMgr::getInstance()->getSession(to);
        if (nullptr == toSession) {
            return;
        }
        // 转发消息给对应客户端
        toSession->asyncSend(data, static_cast<std::uint16_t>(MessageID::ID_NOTIFY_CHAT_MSG));
        status = ChatMsgStatus::IS_SEND;
        return;
    }

}

/*
*{
"uid": 7,
"conv_id": "c2c_0_7",
"conv_type": 1,
"to_uid": 0
}
*/
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
    root["conv_id"] = srcRoot["conv_id"];
    root["conv_type"] = srcRoot["conv_type"];

    // 获取当前时间戳
    auto curTimeStamp = get_current_ms();

    // 写数据库
    const auto uid = srcRoot["uid"].asInt();
    const auto conv_id = srcRoot["conv_id"].asString();
    const auto conv_type = srcRoot["conv_type"].asInt();
    const auto to_uid = srcRoot["to_uid"].asInt();
    if (!MysqlMgr::getInstance()->addConversation(uid, to_uid, conv_id, conv_type)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::MYSQL_ERROR);
        return;
    }

    // 写缓存，创建的时候没有消息，所以最新消息时间先设为 0
    if (!RedisMgr::getInstance()->zSet(CHAT_CONVER_PREFIX + std::to_string(uid), 0, conv_id)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::REDIS_ERROR);
        return;
    }
    // 写会话详情
    std::unordered_map<std::string, std::string> convInfo;
    convInfo["conv_id"] = conv_id;
    convInfo["conv_type"] = std::to_string(conv_type);
    convInfo["to_uid"] = std::to_string(to_uid);
    convInfo["unread_count"] = "0";
    convInfo["last_time"] = ms_to_datetime(curTimeStamp);
    convInfo["is_top"] = "0";
    convInfo["is_mute"] = "0";
    if (!RedisMgr::getInstance()->hSet(CHAT_CONVER_INFO_PREFIX + conv_id, convInfo)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::REDIS_ERROR);
    }
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

void ChatLogicSystem::heartbeatHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
    const std::string &data) {
    Json::Value root;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_HEART_BEAT_RSP));
    });

    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
}


