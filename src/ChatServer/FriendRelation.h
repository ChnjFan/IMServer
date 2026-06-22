//
// Created by Fan on 2026/6/21.
//

#ifndef IMSERVER_FRIENDRELATION_H
#define IMSERVER_FRIENDRELATION_H

#include <string>
#include "UserInfo.h"

enum class FriendStatus : uint8_t {
    NOT_FRIEND = 0,
    FRIEND_PRESENT = 1,
    FRIEND_BACKLIST = 2,
    FRIEND_DELETED = 3,
    FRIEND_HIDDEN = 4,
};

enum class FriendApplyStatus : uint8_t {
    WAIT_AUTH = 0,
    AGREE = 1,
    REJECT = 2,
    EXPIRED = 3,
};

struct FriendRelation {
    int id = 0;
    uint8_t status = 0;         // 好友状态，1-正常；2-拉黑；3-删除；4-屏蔽消息
    uint8_t isStar = 0;    // 星标好友
    uint8_t isHide = 0;    // 隐藏好友
    uint8_t pad;
    std::string alias;      //  备注名
    std::string createTime;
    std::string updateTime;

    void fromJson(const Json::Value& root) {
        if (root.isMember("id")) {
            id = root["id"].asInt();
        }
        if (root.isMember("status")) {
            status = root["status"].asInt();
        }
        if (root.isMember("is_star")) {
            isStar = root["is_star"].asInt();
        }
        if (root.isMember("is_hide")) {
            isHide = root["is_hide"].asInt();
        }
        if (root.isMember("alias")) {
            alias = root["alias"].asString();
        }
        if (root.isMember("create_time")) {
            updateTime = root["create_time"].asString();
        }
        if (root.isMember("update_time")) {
            updateTime = root["update_time"].asString();
        }
    }

    void toJson(Json::Value& root) const {
        root["id"] = id;
        root["status"] = status;
        root["is_star"] = isStar;
        root["is_hide"] = isHide;
        if (!alias.empty()) {
            root["alias"] = alias;
        }
        if (!updateTime.empty()) {
            root["update_time"] = updateTime;
        }
        if (!createTime.empty()) {
            root["create_time"] = createTime;
        }
    }
};

struct FriendInfo {
    int uid = -1;
    int friendId = -1;
    UserFullInfo userInfo;      // 好友用户详细信息
    FriendRelation relation;    // 好友关系

    void fromJson(const Json::Value& root) {
        if (root.isMember("uid")) {
            uid = std::stoi(root["uid"].asString());
        }
        if (root.isMember("friend_id")) {
            friendId = std::stoi(root["friend_id"].asString());
        }
        userInfo.fromJson(root);
        // 更新好友用户信息
        userInfo.baseInfo.uid = friendId;
        relation.fromJson(root);
    }

    void toJson(Json::Value& root) const {
        userInfo.toJson(root);
        relation.toJson(root);
    }
};

struct FriendApplyInfo {
    UserInfo userInfo;
    int id = 0;
    int uid = -1;
    int friendId = -1;
    int status = 0;
    std::string msg;
    std::string expiresTime;
    std::string createdTime;
    std::string updateTime;

    void fromJson(const Json::Value& root) {
        userInfo.fromJson(root);
        if (root.isMember("id")) {
            id = root["id"].asInt();
        }
        if (root.isMember("uid")) {
            uid = std::stoi(root["uid"].asString());
        }
        if (root.isMember("friend_id")) {
            friendId = std::stoi(root["friend_id"].asString());
        }
        if (root.isMember("status")) {
            status = root["status"].asInt();
        }
        if (root.isMember("message")) {
            msg = root["message"].asString();
        }
        if (root.isMember("expires_time")) {
            expiresTime = root["expires_time"].asString();
        }
        if (root.isMember("created_time")) {
            createdTime = root["created_time"].asString();
        }
        if (root.isMember("update_time")) {
            updateTime = root["update_time"].asString();
        }
    }

    void toJson(Json::Value& root) const {
        userInfo.toJson(root);
        root["id"] = id;
        if (uid >= 0) {
            root["uid"] = std::to_string(uid);
        }
        if (friendId >= 0) {
            root["friend_id"] = std::to_string(friendId);
        }
        root["status"] = status;
        root["message"] = msg;
        root["expires_time"] = expiresTime;
        root["created_time"] = createdTime;
        root["update_time"] = updateTime;
    }
};

#endif //IMSERVER_FRIENDRELATION_H