//
// Created by Fan on 2026/6/24.
//

#ifndef IMSERVER_FRIENDINFO_H
#define IMSERVER_FRIENDINFO_H

#include <string>
#include <unordered_set>
#include <json/json.h>
#include <jdbc/cppconn/resultset.h>

enum class FriendStatus : uint8_t {
    NOT_FRIEND = 0,
    FRIEND_PRESENT = 1,
    FRIEND_BACKLIST = 2,
    FRIEND_DELETED = 3,
    FRIEND_HIDDEN = 4,
};

struct FriendInfo {
    int friendId = -1;
    int8_t status = -1;
    int8_t isStar = -1;
    int8_t isHide = -1;
    int8_t pad;
    std::optional<std::string> name;
    std::optional<std::string> alias;
    std::optional<std::string> email;
    std::optional<std::string> avatarUrl;
    std::optional<std::string> createTime;
    std::optional<std::string> updateTime;

    inline static std::unordered_set<std::string> keys{"friend_id", "status",
    "is_star", "is_hide", "alias", "create_time", "update_time"};

    static FriendInfo fromFriendListSearch(const std::shared_ptr<sql::ResultSet>& result);

    void fromJson(Json::Value& value);
    void toJson(Json::Value& value) const;

    [[nodiscard]] std::string getUpdateProperty() const;
    [[nodiscard]] bool getUpdatePropertyStringValue(std::string& value) const;
    [[nodiscard]] bool getUpdatePropertyIntValue(int& value) const;
    //
    // // 判断 KEY 是否是 user_profile 字段
    // static bool isProperty(const std::string& value);
};

inline FriendInfo FriendInfo::fromFriendListSearch(const std::shared_ptr<sql::ResultSet> &result) {
    FriendInfo info{};
    info.friendId = std::stoi(result->getString("friend_id"));
    info.status = static_cast<int8_t>(result->getInt("status"));
    info.isStar = static_cast<int8_t>(result->getInt("is_star"));
    info.alias = result->getString("alias");
    info.name = result->getString("name");
    info.email = result->getString("email");
    info.avatarUrl = result->getString("avatar");
    info.createTime = result->getString("create_time");
    info.updateTime = result->getString("update_time");
    return info;
}

inline void FriendInfo::fromJson(Json::Value &value) {
    if (value.isMember("friend_id")) {
        friendId = std::stoi(value["friend_id"].asString());
    }
    if (value.isMember("status")) {
        status = static_cast<int8_t>(value["status"].asInt());
    }
    if (value.isMember("is_star")) {
        isStar = static_cast<int8_t>(value["is_star"].asInt());
    }
    if (value.isMember("alias")) {
        alias = value["alias"].asString();
    }
    if (value.isMember("name")) {
        name = value["name"].asString();
    }
    if (value.isMember("email")) {
        email = value["email"].asString();
    }
    if (value.isMember("avatar_url")) {
        avatarUrl = value["avatar_url"].asString();
    }
    if (value.isMember("create_time")) {
        createTime = value["create_time"].asString();
    }
    if (value.isMember("update_time")) {
        updateTime = value["update_time"].asString();
    }
}

inline void FriendInfo::toJson(Json::Value& value) const {
    if (friendId < 0) {
        throw std::invalid_argument("invalid uid");
    }

    value["friend_id"] = std::to_string(friendId);
    if (status >= 0) {
        value["status"] = status;
    }
    if (isStar >= 0) {
        value["is_star"] = isStar;
    }
    if (name.has_value()) {
        value["name"] = name.value();
    }
    if (email.has_value()) {
        value["email"] = email.value();
    }
    if (alias.has_value()) {
        value["alias"] = alias.value();
    }
    if (avatarUrl.has_value()) {
        value["avatar_url"] = avatarUrl.value();
    }
    if (createTime.has_value()) {
        value["create_time"] = createTime.value();
    }
    if (updateTime.has_value()) {
        value["update_time"] = updateTime.value();
    }
}

inline std::string FriendInfo::getUpdateProperty() const {
    if (status >= 0) {
        return "status";
    }
    if (isStar >= 0) {
        return "is_star";
    }
    if (isHide >= 0) {
        return "is_hide";
    }
    if (alias.has_value()) {
        return "alias";
    }
    throw std::invalid_argument("invalid update friend info");
}

inline bool FriendInfo::getUpdatePropertyStringValue(std::string& value) const {
    if (alias.has_value()) {
        value = alias.value();
        return true;
    }
    return false;
}

inline bool FriendInfo::getUpdatePropertyIntValue(int& value) const {
    if (status >= 0) {
        value = static_cast<int>(status);
        return true;
    }
    if (isStar >= 0) {
        value = static_cast<int>(isStar);
        return true;
    }
    if (isHide >= 0) {
        value = static_cast<int>(isHide);
        return true;
    }
    return false;
}

#endif //IMSERVER_FRIENDINFO_H
