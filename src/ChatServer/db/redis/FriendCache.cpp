//
// Created by Fan on 2026/6/24.
//

#include "FriendCache.h"

bool FriendCache::isFriend(const int uid, const int friendId) {
    if (uid < 0 || friendId < 0) {
        std::cout << "[isFriend] Input invalid param" << std::endl;
        return false;
    }

    return RedisMgr::getInstance()->sIsMember(FRIEND_SET_PREFIX + std::to_string(uid),
        std::to_string(friendId));
}

bool FriendCache::updateFriendSet(const int uid, const int friendId) {
    if (uid < 0 || friendId < 0) {
        std::cout << "[isFriend] Input invalid param" << std::endl;
        return false;
    }

    return RedisMgr::getInstance()->sAdd(FRIEND_SET_PREFIX + std::to_string(uid), std::to_string(friendId));
}

bool FriendCache::deleteFriendSet(const int uid, const int friendId) {
    if (uid < 0 || friendId < 0) {
        std::cout << "[isFriend] Input invalid param" << std::endl;
        return false;
    }

    return RedisMgr::getInstance()->sRem(FRIEND_SET_PREFIX + std::to_string(uid), std::to_string(friendId));
}

bool FriendCache::getFriendInfo(const int uid, const int friendId, FriendInfo &info) {
    const std::string res = RedisMgr::getInstance()->hGet(FRIEND_RELATION_INFO_PREFIX + std::to_string(uid),
            std::to_string(friendId));
    if (res.empty()) {
        return false;
    }

    Json::Value root;
    if (Json::Reader reader; reader.parse(res, root)) {
        info.fromJson(root);
    }
    return true;
}

void FriendCache::clearFriendApplyCount(const int uid) {
    RedisMgr::getInstance()->hDel(USER_COUNTER_PREFIX + std::to_string(uid),
            USER_FRIEND_REPLY_COUNT);
}

bool FriendCache::getFriendApplyCount(const int uid, int &count) {
    const auto res = RedisMgr::getInstance()->hGet(USER_COUNTER_PREFIX + std::to_string(uid),
        USER_FRIEND_REPLY_COUNT);
    if (res.empty()) {
        return false;
    }

    count = std::stoi(res);
    return true;
}

bool FriendCache::updateFriendApplyCount(const int uid, const int count) {
    if (uid < 0 || count < 0) {
        std::cout << "[updateFriend] Input invalid param" << std::endl;
        return false;
    }

    return RedisMgr::getInstance()->hSet(USER_COUNTER_PREFIX + std::to_string(uid),
        USER_FRIEND_REPLY_COUNT, std::to_string(count));
}
