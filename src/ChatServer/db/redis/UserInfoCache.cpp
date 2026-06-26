//
// Created by Fan on 2026/6/23.
//

#include <iostream>

#include "RedisMgr.h"
#include "UserInfoCache.h"


bool UserInfoCache::getBaseInfo(const int uid, UserBaseInfo &info) {
    if (uid < 0) {
        std::cout << "[getBaseInfo] Input invalid param" << std::endl;
        return false;
    }

    if (std::string res; RedisMgr::getInstance()->get(USER_BASE_INFO_PREFIX + std::to_string(uid), res)) {
        Json::Value root;
        if (Json::Reader reader; !reader.parse(res, root)) {
            std::cout << "Failed to parse JSON data" << std::endl;
            return false;
        }
        info.fromJson(root);
        return true;
    }
    return false;
}

bool UserInfoCache::updateBaseInfo(const UserBaseInfo &info) {
    Json::Value root;
    info.toJson(root);
    return RedisMgr::getInstance()->set(USER_BASE_INFO_PREFIX + std::to_string(info.uid), root.toStyledString());
}

bool UserInfoCache::getUserProfile(const int uid, UserProfile &profile) {
    if (uid < 0) {
        std::cout << "[getBaseInfo] Input invalid param" << std::endl;
        return false;
    }

    if (std::string res; RedisMgr::getInstance()->get(USER_PROFILE_INFO_PREFIX + std::to_string(uid), res)) {
        Json::Value root;
        if (Json::Reader reader; !reader.parse(res, root)) {
            std::cout << "Failed to parse JSON data" << std::endl;
            return false;
        }
        profile.fromJson(root);
        return true;
    }
    return false;
}

int UserInfoCache::searchUid(UserBaseInfo &info) {
    if (info.uid != -1) {   // 有 UID 直接返回，否则去 Redis 中查询
        return info.uid;
    }
    const std::string key = UID_INDEX_MAP_PREFIX + info.getSearchProperty();
    if (std::string uid; RedisMgr::getInstance()->get(key, uid)) {
        info.uid = std::stoi(uid);
        return info.uid;
    }
    return -1;
}

bool UserInfoCache::updateUidMap(const UserBaseInfo &info) {
    bool ret = true;

    if (info.uid < 0) {
        std::cout << "[updateUidMap] Input invalid param" << std::endl;
        return false;
    }

    if (!info.email.empty()) {
        ret |= RedisMgr::getInstance()->set(UID_INDEX_MAP_PREFIX + info.email, std::to_string(info.uid));
    }
    if (!info.name.empty()) {
        ret |= RedisMgr::getInstance()->set(UID_INDEX_MAP_PREFIX + info.name, std::to_string(info.uid));
    }
    return ret;
}

bool UserInfoCache::searchUserBaseInfo(UserBaseInfo &info) {
    if (info.uid == -1 || searchUid(info) == -1) {
        return false;
    }
    return getBaseInfo(info.uid, info);
}

bool UserInfoCache::searchUserFullInfo(UserBaseInfo &info, UserProfile &profile) {
    if (!searchUserBaseInfo(info)) {
        return false;
    }

    return getUserProfile(info.uid, profile);
}


