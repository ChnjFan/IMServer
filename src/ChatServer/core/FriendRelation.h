//
// Created by Fan on 2026/6/21.
//

#ifndef IMSERVER_FRIENDRELATION_H
#define IMSERVER_FRIENDRELATION_H

#include <string>
#include "UserInfo.h"

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


#endif //IMSERVER_FRIENDRELATION_H