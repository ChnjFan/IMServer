//
// Created by Fan on 2026/6/25.
//

#ifndef IMSERVER_FRIENDAPPLY_H
#define IMSERVER_FRIENDAPPLY_H

#include <string>
#include <unordered_set>
#include <json/json.h>
#include <jdbc/cppconn/resultset.h>

enum class FriendApplyStatus : uint8_t {
    NORMAL = 0,
    ACCESS = 1,
    REJECT = 2,
    EXPIRE = 3,
};

struct FriendApply {
    int uid = -1;           // 发起申请人
    int friendId = -1;      // 待添加人
    int8_t status = -1;
    int8_t gender = -1;
    int8_t pad[2];
    std::optional<std::string> msg; // 申请消息
    std::string name;       // 申请好友对方名字
    std::string email;      // 申请好友对方邮箱
    std::string createTime;
    std::string expireTime;
    std::string updateTime;

    inline static std::unordered_set<std::string> keys{"id", "uid", "friend_id", "status",
    "msg", "createTime", "expireTime", "updateTime"};

    static FriendApply fromFriendApplySearch(const std::shared_ptr<sql::ResultSet>& result);

    void fromJson(Json::Value& value);
    void toJson(Json::Value& value) const;

    // [[nodiscard]] std::string getSearchProperty() const;
    // [[nodiscard]] std::string getSearchPropertyStringValue() const;
    // [[nodiscard]] int getSearchPropertyIntValue() const;
    //
    // // 判断 KEY 是否是 user_profile 字段
    // static bool isProperty(const std::string& value);
};

inline FriendApply FriendApply::fromFriendApplySearch(const std::shared_ptr<sql::ResultSet> &result) {
    FriendApply info{};
    info.uid = result->getInt("uid");
    info.friendId = result->getInt("friend_id");
    info.name = result->getString("name");
    info.email = result->getString("email");
    info.status = static_cast<int8_t>(result->getInt("status"));
    info.gender = static_cast<int8_t>(result->getInt("gender"));
    info.msg = result->getString("msg");
    info.createTime = result->getString("create_time");
    info.expireTime = result->getString("expire_time");
    info.updateTime = result->getString("update_time");
    return info;
}

inline void FriendApply::fromJson(Json::Value &value) {
    if (value.isMember("uid")) {
        uid = std::stoi(value["uid"].asString());
    }
    if (value.isMember("friend_id")) {
        friendId = std::stoi(value["friend_id"].asString());
    }
    if (value.isMember("status")) {
        status = static_cast<int8_t>(value["status"].asInt());
    }
    if (value.isMember("message")) {
        msg = value["message"].asString();
    }
    if (value.isMember("create_time")) {
        createTime = value["create_time"].asString();
    }
    if (value.isMember("expire_time")) {
        expireTime = value["expire_time"].asString();
    }
    if (value.isMember("update_time")) {
        updateTime = value["update_time"].asString();
    }
}

inline void FriendApply::toJson(Json::Value& value) const {
    if (friendId < 0) {
        throw std::invalid_argument("invalid uid");
    }

    value["uid"] = std::to_string(uid);
    value["friend_id"] = std::to_string(friendId);
    if (!name.empty()) {
        value["name"] = name;
    }
    if (!email.empty()) {
        value["email"] = email;
    }
    if (status >= 0) {
        value["status"] = status;
    }
    if (gender >= 0) {
        value["gender"] = gender;
    }
    if (msg.has_value()) {
        value["message"] = msg.value();
    }
    value["create_time"] = createTime;
    value["expire_time"] = expireTime;
    value["update_time"] = updateTime;
}

#endif //IMSERVER_FRIENDAPPLY_H