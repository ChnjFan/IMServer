//
// Created by Fan on 2026/6/24.
//

#ifndef IMSERVER_FRIENDCACHE_H
#define IMSERVER_FRIENDCACHE_H

#include <Singleton.h>

#include "RedisMgr.h"
#include "common/model/FriendInfo.h"
#include "common/lru/LruCache.h"

#define FRIEND_LOCAL_CACHE_PREFIX "IS_FRIEND:"
// 好友关系
#define FRIEND_SET_PREFIX "friend_set_"
#define FRIEND_RELATION_INFO_PREFIX "friend_relation_info_"
// 状态计数
#define USER_FRIEND_REPLY_COUNT "friend_reply"  // 好友申请计数

class FriendCache : public Singleton<FriendCache> {
    friend class Singleton<FriendCache>;
public:
    ~FriendCache() = default;

     [[nodiscard]] bool isFriend(int uid, int friendId) const;
     bool updateFriendSet(int uid, int friendId);
     bool deleteFriendSet(int uid, int friendId);
     bool getFriendInfo(int uid, int friendId, FriendInfo& info);

     void clearFriendApplyCount(int uid);
     bool getFriendApplyCount(int uid, int& count);
     bool updateFriendApplyCount(int uid, int count);

private:
    FriendCache();

    static std::pair<std::string, std::string> splitFriendCacheKey(const std::string& key);
    std::shared_ptr<LruCache<FriendInfo>> friendLocalCache_;
};

#endif //IMSERVER_FRIENDCACHE_H
