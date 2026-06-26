//
// Created by Fan on 2026/6/23.
//

#ifndef IMSERVER_USERPROFILE_H
#define IMSERVER_USERPROFILE_H

#include <string>
#include <unordered_set>
#include <memory>
#include <json/json.h>
#include <jdbc/cppconn/resultset.h>

enum class PrivacyFriendLevel : uint8_t {
    FORBID = 0,     // 禁止添加
    NEED_AUTH = 1,  // 需要认证
    ADD_DIRECT = 2, // 直接添加
};

struct UserProfile {
    int uid = -1;
    int8_t privacyFriend = -1;     // 好友添加权限：PrivacyFriendLevel
    int8_t privacyChat = -1;       // 陌生人私信权限。0/1
    int8_t blacklistSwitch = -1;   // 黑名单开启。0/1
    int8_t pad = -1;
    std::optional<std::string> birthday;
    std::optional<std::string> region;
    std::optional<std::string> signature;
    std::optional<std::string> selfIntro;
    std::optional<std::string> extraJson;

    inline static std::unordered_set<std::string> keys{"uid", "privacy_friend", "privacy_chat",
    "blacklist_switch", "birthday", "region", "signature", "self_intro", "extra"};

    void fromJson(const Json::Value& value);
    void fromSqlResult(const std::shared_ptr<sql::ResultSet>& result);
    void toJson(Json::Value& value) const;

    // 判断 KEY 是否是 user_profile 字段
    static bool isProperty(const std::string& value);
};

inline void UserProfile::fromJson(const Json::Value &value) {
    if (value.isMember("uid") && !value["uid"].isNull()) {
        uid = std::stoi(value["uid"].asString());
    }
    if (value.isMember("privacy_friend") && !value["privacy_friend"].isNull()) {
        privacyFriend = static_cast<int8_t>(value["privacy_friend"].asUInt());
    }
    if (value.isMember("privacy_chat") && !value["privacy_chat"].isNull()) {
        privacyChat = static_cast<int8_t>(value["privacy_chat"].asUInt());
    }
    if (value.isMember("blacklist_switch") && !value["blacklist_switch"].isNull()) {
        blacklistSwitch = static_cast<int8_t>(value["blacklist_switch"].asUInt());
    }
    if (value.isMember("birthday") && !value["birthday"].isNull()) {
        birthday = value["birthday"].asString();
    }
    if (value.isMember("region") && !value["region"].isNull()) {
        region = value["region"].asString();
    }
    if (value.isMember("signature") && !value["signature"].isNull()) {
        signature = value["signature"].asString();
    }
    if (value.isMember("self_intro") && !value["self_intro"].isNull()) {
        selfIntro = value["self_intro"].asString();
    }
    if (value.isMember("extra") && !value["extra"].isNull()) {
        extraJson = value["extra"].asString();
    }
}

inline void UserProfile::fromSqlResult(const std::shared_ptr<sql::ResultSet>& result) {
    if (!result->isNull("uid")) {
        uid = result->getInt("uid");
    }
    if (!result->isNull("privacy_friend")) {
        privacyFriend = static_cast<int8_t>(result->getInt("privacy_friend"));
    }
    if (!result->isNull("privacy_chat")) {
        privacyChat = static_cast<int8_t>(result->getInt("privacy_chat"));
    }
    if (!result->isNull("blacklist_switch")) {
        blacklistSwitch = static_cast<int8_t>(result->getInt("blacklist_switch"));
    }
    if (!result->isNull("birthday")) {
        birthday = result->getString("birthday");
    }
    if (!result->isNull("region")) {
        region = result->getString("region");
    }
    if (!result->isNull("signature")) {
        signature = result->getString("signature");
    }
    if (!result->isNull("self_intro")) {
        selfIntro = result->getString("self_intro");
    }
    if (!result->isNull("extra_json")) {
        extraJson = result->getString("extra_json");
    }
}

inline void UserProfile::toJson(Json::Value &value) const {
    if (uid < 0) {
        throw std::invalid_argument("invalid uid");
    }

    value["uid"] = uid;
    if (privacyFriend >= 0) {
        value["privacy_friend"] = privacyFriend;
    }
    if (privacyChat >= 0) {
        value["privacy_chat"] = privacyChat;
    }
    if (blacklistSwitch >= 0) {
        value["blacklist_switch"] = blacklistSwitch;
    }
    if (birthday.has_value()) {
        value["birthday"] = birthday.value();
    }
    if (region.has_value()) {
        value["region"] = region.value();
    }
    if (signature.has_value()) {
        value["signature"] = signature.value();
    }
    if (selfIntro.has_value()) {
        value["self_intro"] = selfIntro.value();
    }
    if (extraJson.has_value()) {
        value["extra"] = extraJson.value();
    }
}

inline bool UserProfile::isProperty(const std::string &value) {
    return keys.find(value) != keys.end();
}

#endif //IMSERVER_USERPROFILE_H
