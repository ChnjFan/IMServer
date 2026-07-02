//
// Created by Fan on 2026/6/27.
//

#ifndef IMSERVER_CONVERSATIONINFO_H
#define IMSERVER_CONVERSATIONINFO_H

#include <string>
#include <unordered_set>
#include <regex>
#include <json/json.h>
#include <jdbc/cppconn/resultset.h>

#include "common/utils/ConversationConvert.h"

enum class ConvType : int8_t {
    PRIVATE_CHAT = 1,
    GROUP_CHAT = 2,
};

struct ConversationInfo {
    mutable int uid = -1;           // 发起申请人
    int friendId = -1;      // 好友 ID
    int unreadCount = -1;   // 未读消息计数
    int lastMsgId = -1;     // 最新消息 ID
    int lastReadMsgId = -1; // 最新已读消息 ID
    int8_t convType = -1;   // 会话类型 1-私聊；2-群聊
    int8_t status = -1;     // 状态 0-正常；1-已解散
    int8_t isTop = -1;      // 置顶
    int8_t isMute = -1;     // 免打扰
    std::string convId;     // 会话 ID，c2c_ / group_
    std::optional<std::string> lastMsgContent; // 最新消息摘要，最大 15 个字符
    std::optional<std::string> lastTime; // 最新消息时间
    std::optional<std::string> title;
    std::optional<std::string> createTime;
    std::optional<std::string> updateTime;

    inline static std::unordered_set<std::string> keys{"uid", "unread_count", "status",
    "last_msg_content", "last_msg_id", "last_read_msg_id", "conv_type", "is_top", "is_mute",
    "conv_id", "last_time", "update_time"};

    // static FriendApply fromFriendApplySearch(const std::shared_ptr<sql::ResultSet>& result);

    void fromJson(Json::Value& value);
    void toJson(Json::Value& value) const;

    static ConversationInfo fromConversationListSearch(const std::shared_ptr<sql::ResultSet>& result);

    void generateConvId();
    [[nodiscard]] int getOtherUid() const;

    // [[nodiscard]] std::string getSearchProperty() const;
    // [[nodiscard]] std::string getSearchPropertyStringValue() const;
    // [[nodiscard]] int getSearchPropertyIntValue() const;
    //
    // // 判断 KEY 是否是 ConversationInfo 字段
    // static bool isProperty(const std::string& value);
};

inline void ConversationInfo::fromJson(Json::Value &value) {
    if (value.isMember("uid")) {
        uid = std::stoi(value["uid"].asString());
    }
    if (value.isMember("friend_id")) {
        friendId = std::stoi(value["friend_id"].asString());
    }
    if (value.isMember("unread_count")) {
        unreadCount = value["unread_count"].asInt();
    }
    if (value.isMember("status")) {
        status = static_cast<int8_t>(value["status"].asInt());
    }
    if (value.isMember("last_msg_content")) {
        lastMsgContent = value["last_msg_content"].asString();
    }
    if (value.isMember("last_msg_id")) {
        lastMsgId = value["last_msg_id"].asInt();
    }
    if (value.isMember("last_read_msg_id")) {
        lastReadMsgId = value["last_read_msg_id"].asInt();
    }
    if (value.isMember("last_time")) {
        lastTime = value["last_time"].asString();
    }
    if (value.isMember("conv_type")) {
        convType = static_cast<int8_t>(value["conv_type"].asInt());
    }
    if (value.isMember("is_top")) {
        isTop = static_cast<int8_t>(value["is_top"].asInt());
    }
    if (value.isMember("is_mute")) {
        isMute = static_cast<int8_t>(value["is_mute"].asInt());
    }
    if (value.isMember("conv_id")) {
        convId = value["conv_id"].asString();
    }
    if (value.isMember("update_time")) {
        updateTime = value["update_time"].asString();
    }
    if (value.isMember("createTime")) {
        createTime = value["createTime"].asString();
    }
    if (value.isMember("title")) {
        title = value["title"].asString();
    }
}

inline void ConversationInfo::toJson(Json::Value &value) const {
    if (uid >= 0) {
        value["uid"] = std::to_string(uid);
    }
    if (unreadCount >= 0) {
        value["unread_count"] = unreadCount;
    }
    if (status >= 0) {
        value["status"] = status;
    }
    if (isTop >= 0) {
        value["is_top"] = isTop;
    }
    if (isMute >= 0) {
        value["is_mute"] = isMute;
    }
    if (!convId.empty()) {
        value["conv_id"] = convId;
    }
    if (convType >= 0) {
        value["conv_type"] = convType;
    }
    if (lastMsgId >= 0) {
        value["last_msg_id"] = lastMsgId;
    }
    if (lastReadMsgId >= 0) {
        value["last_read_msg_id"] = lastReadMsgId;
    }
    if (lastMsgContent.has_value()) {
        value["last_msg_content"] = lastMsgContent.value();
    }
    if (updateTime.has_value()) {
        value["update_time"] = updateTime.value();
    }
    if (createTime.has_value()) {
        value["create_time"] = createTime.value();
    }
    if (lastTime.has_value()) {
        value["last_time"] = lastTime.value();
    }
    if (title.has_value()) {
        value["title"] = title.value();
    }
}

inline ConversationInfo ConversationInfo::fromConversationListSearch(const std::shared_ptr<sql::ResultSet> &result) {
    ConversationInfo info;
    info.convId = result->getString("conv_id");
    info.convType = static_cast<int8_t>(result->getUInt("conv_type"));
    info.lastMsgId = result->getInt("last_msg_id");
    info.lastMsgContent = result->getString("last_msg_content");
    info.lastTime = result->getString("last_time");
    info.updateTime = result->getString("update_time");
    info.createTime = result->getString("create_time");
    info.unreadCount = result->getInt("unread_count");
    info.isTop = static_cast<int8_t>(result->getUInt("is_top"));
    info.isMute = static_cast<int8_t>(result->getUInt("is_mute"));
    return info;
}

inline void ConversationInfo::generateConvId() {
    if (convType < 0) {
        return;
    }

    switch (static_cast<ConvType>(convType)) {
        case ConvType::PRIVATE_CHAT: {
            const std::string uidStr = uid < friendId ?
                std::to_string(uid) + "_" + std::to_string(friendId) :
                std::to_string(friendId) + "_" + std::to_string(uid);
            convId = std::string("c2c_") + uidStr;
            break;
        }
        case ConvType::GROUP_CHAT: {
            convId = std::string("group_") + std::to_string(uid);
            break;
        }
        default: {
            break;
        }
    }
}

inline int ConversationInfo::getOtherUid() const {
    return ::getOtherUid(convId, uid);
}

#endif //IMSERVER_CONVERSATIONINFO_H
