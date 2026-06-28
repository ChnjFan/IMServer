//
// Created by Fan on 2026/6/24.
//

#ifndef IMSERVER_FRIENDCACHE_H
#define IMSERVER_FRIENDCACHE_H

#include <Singleton.h>

#include "RedisMgr.h"
#include "common/model/FriendInfo.h"

// 好友关系
#define FRIEND_SET_PREFIX "friend_set_"
#define FRIEND_RELATION_INFO_PREFIX "friend_relation_info_"
// 状态计数
#define USER_FRIEND_REPLY_COUNT "friend_reply"  // 好友申请计数

class FriendCache : public Singleton<FriendCache> {
    friend class Singleton<FriendCache>;
public:
    ~FriendCache() = default;

    static bool isFriend(int uid, int friendId);
    static bool updateFriendSet(int uid, int friendId);
    static bool deleteFriendSet(int uid, int friendId);
    static bool getFriendInfo(int uid, int friendId, FriendInfo& info);

    static void clearFriendApplyCount(int uid);
    static bool getFriendApplyCount(int uid, int& count);
    static bool updateFriendApplyCount(int uid, int count);

private:
    FriendCache() = default;
};

#endif //IMSERVER_FRIENDCACHE_H
