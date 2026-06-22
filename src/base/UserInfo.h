//
// Created by Fan on 2026/6/20.
//

#ifndef IMSERVER_USERINFO_H
#define IMSERVER_USERINFO_H

#include <string>
#include <json/json.h>

struct UserInfo {
    int uid = 0;
    int gender = 0;
    std::string name;
    std::string email;
    std::string avatarUrl;
    std::string password;
    std::string createTime;
    std::string birthday;
    std::string region;
    std::string signature;
    std::string selfIntro;

    UserInfo() : uid(-1) {};

    void fromJson(const Json::Value& json) {
        if (json.isMember("uid")) {
            this->uid = std::stoi(json["uid"].asString());
        }
        if (json.isMember("gender")) {
            this->gender = json["gender"].asInt();
        }
        if (json.isMember("name")) {
            this->name = json["name"].asString();
        }
        if (json.isMember("email")) {
            this->email = json["email"].asString();
        }
        if (json.isMember("avatar_url")) {
            this->password = json["avatarUrl"].asString();
        }
        if (json.isMember("password")) {
            this->password = json["password"].asString();
        }
        if (json.isMember("create_time")) {
            this->createTime = json["create_time"].asString();
        }
        if (json.isMember("birthday")) {
            this->birthday = json["birthday"].asString();
        }
        if (json.isMember("region")) {
            this->region = json["region"].asString();
        }
        if (json.isMember("signature")) {
            this->signature = json["signature"].asString();
        }
        if (json.isMember("self_intro")) {
            this->selfIntro = json["self_intro"].asString();
        }
    }

    void toJson(Json::Value& root, const bool needPwd = false) const {
        if (uid < 0) {
            return;
        }
        root["uid"] = std::to_string(this->uid);
        root["gender"] = this->gender;
        root["name"] = this->name;
        root["email"] = this->email;
        root["avatar_url"] = this->avatarUrl;
        root["create_time"] = this->createTime;
        if (!birthday.empty()) {
            root["birthday"] = this->birthday;
        }
        if (!region.empty()) {
            root["region"] = this->region;
        }
        if (!signature.empty()) {
            root["signature"] = this->signature;
        }
        if (!selfIntro.empty()) {
            root["self_intro"] = this->selfIntro;
        }
        if (needPwd) {
            root["password"] = this->password;
        }
    }
};

struct UserProfileInfo {
    int friendStatus = 0;
    int privacyFriend = 0;
    int privacyChat = 0;
    int privacyBacklist = 0;
    std::string phone;
    std::string birthday;
    std::string signature;
    std::string region;
    std::string selfIntro;
    std::string ex;
    std::string createTime;
    std::string updateTime;

    void fromJson(const Json::Value &json) {
        if (json.isMember("friend_status")) {
            this->friendStatus = json["friend_status"].asInt();
        }
        if (json.isMember("privacy_friend")) {
            this->privacyFriend = json["privacy_friend"].asInt();
        }
        if (json.isMember("privacy_chat")) {
            this->privacyChat = json["privacy_chat"].asInt();
        }
        if (json.isMember("privacy_blacklist")) {
            this->privacyBacklist = json["privacy_blacklist"].asInt();
        }
        if (json.isMember("phone")) {
            this->phone = json["phone"].asString();
        }
        if (json.isMember("birthday")) {
            this->birthday = json["birthday"].asString();
        }
        if (json.isMember("signature")) {
            this->signature = json["signature"].asString();
        }
        if (json.isMember("region")) {
            this->region = json["region"].asString();
        }
        if (json.isMember("self_intro")) {
            this->selfIntro = json["self_intro"].asString();
        }
        if (json.isMember("ex")) {
            this->ex = json["ex"].asString();
        }
        if (json.isMember("create_time")) {
            this->updateTime = json["create_time"].asString();
        }
        if (json.isMember("update_time")) {
            this->updateTime = json["update_time"].asString();
        }
    }

    void toJson(Json::Value& root) const {
        root["friend_status"] = this->friendStatus;
        root["privacy_friend"] = this->privacyFriend;
        root["privacy_chat"] = this->privacyChat;
        root["privacy_blacklist"] = this->privacyBacklist;
        root["phone"] = this->phone;
        root["birthday"] = this->birthday;
        root["signature"] = this->signature;
        root["region"] = this->region;
        root["self_intro"] = this->selfIntro;
        root["ex"] = this->ex;
        root["create_time"] = this->updateTime;
        root["update_time"] = this->updateTime;
    }
};

struct UserFullInfo {
    UserInfo baseInfo;
    UserProfileInfo profileInfo;

    void fromJson(const Json::Value &json) {
        baseInfo.fromJson(json);
        profileInfo.fromJson(json);
    }

    void toJson(Json::Value& root, const bool needPwd = false) const {
        baseInfo.toJson(root, needPwd);
        profileInfo.toJson(root);
    }
};

#endif //IMSERVER_USERINFO_H