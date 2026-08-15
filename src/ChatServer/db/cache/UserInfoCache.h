//
// Created by Fan on 2026/6/23.
//

#ifndef IMSERVER_USERINFOCACHE_H
#define IMSERVER_USERINFOCACHE_H

#include <Singleton.h>

#include "common/model/UserBaseInfo.h"
#include "common/model/UserProfile.h"

// 用户信息 KEY
#define UID_INDEX_MAP_PREFIX "uid_index_"
#define USER_BASE_INFO_PREFIX "user_base_info_"
#define USER_PROFILE_INFO_PREFIX "user_profile_info_"

class UserInfoCache : public Singleton<UserInfoCache> {
    friend class Singleton<UserInfoCache>;
public:
    ~UserInfoCache() = default;

    static bool getBaseInfo(int uid, UserBaseInfo& info);
    static bool updateBaseInfo(const UserBaseInfo& info);

    static bool getUserProfile(int uid, UserProfile& profile);

    static int searchUid(UserBaseInfo& info);
    static bool updateUidMap(const UserBaseInfo& info);
    static bool searchUserBaseInfo(UserBaseInfo& info);
    static bool searchUserFullInfo(UserBaseInfo& info, UserProfile& profile);

private:
    UserInfoCache() = default;
};




#endif //IMSERVER_USERINFOCACHE_H
