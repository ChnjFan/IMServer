//
// Created by Fan on 2026/6/24.
//

#include "FriendCache.h"
#include "MysqlMgr.h"

bool FriendCache::isFriend(const int uid, const int friendId) const {
    if (uid < 0 || friendId < 0) {
        return false;
    }

    const auto info =  friendLocalCache_->get(FRIEND_LOCAL_CACHE_PREFIX
        + std::to_string(uid) + ":" + std::to_string(friendId));
    return info.has_value();
}

bool FriendCache::updateFriendSet(const int uid, const int friendId) {
    if (uid < 0 || friendId < 0) {
        return false;
    }

    return RedisMgr::getInstance()->sAdd(FRIEND_SET_PREFIX + std::to_string(uid), std::to_string(friendId));
}

bool FriendCache::deleteFriendSet(const int uid, const int friendId) {
    if (uid < 0 || friendId < 0) {
        return false;
    }

    return RedisMgr::getInstance()->sRem(FRIEND_SET_PREFIX + std::to_string(uid), std::to_string(friendId));
}

bool FriendCache::getFriendInfo(const int uid, const int friendId, FriendInfo &info) {
    const auto res =  friendLocalCache_->get(FRIEND_LOCAL_CACHE_PREFIX
        + std::to_string(uid) + ":" + std::to_string(friendId));
    if (!res.has_value()) {
        return false;
    }
    info = res.value();
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
        return false;
    }

    return RedisMgr::getInstance()->hSet(USER_COUNTER_PREFIX + std::to_string(uid),
        USER_FRIEND_REPLY_COUNT, std::to_string(count));
}

FriendCache::FriendCache() {
    friendLocalCache_ = LruCache<FriendInfo>::create(
    {.blockOnExpire = true},
    [](const std::string& key) -> std::optional<FriendInfo> {
        // 加载缓存
        auto [uid, friendId] = splitFriendCacheKey(key);
        auto info = std::make_optional<FriendInfo>();
        if (const std::string res = RedisMgr::getInstance()->hGet(FRIEND_RELATION_INFO_PREFIX + uid, friendId);
            !res.empty()) {
            Json::Value root;
            if (Json::Reader reader; reader.parse(res, root)) {
               info.value().fromJson(root);
            }
            return info;
        }

        if (MysqlMgr::getInstance()->selectFriend(std::stoi(uid), std::stoi(friendId), info.value())) {
            Json::Value root;
            info.value().toJson(root);
            RedisMgr::getInstance()->hSet(FRIEND_RELATION_INFO_PREFIX + uid, friendId, root.toStyledString());
            return info;
        }

        return std::nullopt;
    },
    [](const std::string& key, FriendInfo& val) {}
   );
}

std::pair<std::string, std::string> FriendCache::splitFriendCacheKey(const std::string &key) {
    // key 格式: "IS_FRIEND:uid:friendId"
    const std::string prefix = FRIEND_LOCAL_CACHE_PREFIX;
    std::string_view rest(key.c_str() + prefix.size(), key.size() - prefix.size());

    const auto pos = rest.find(':');
    if (pos == std::string_view::npos) {
        return {};
    }

    return {std::string(rest.substr(0, pos)), std::string(rest.substr(pos + 1))};
}
