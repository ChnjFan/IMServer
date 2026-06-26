//
// Created by Fan on 2026/6/23.
//

#ifndef IMSERVER_USERINFODAO_H
#define IMSERVER_USERINFODAO_H

#include "MysqlPool.h"
#include "common/model/UserBaseInfo.h"
#include "common/model/UserProfile.h"

class UserInfoDao {
public:
    UserInfoDao();
    ~UserInfoDao();

    [[nodiscard]] std::vector<UserBaseInfo> selectUserListInfo(const UserBaseInfo& searchInfo) const;

    bool selectUserBaseInfo(UserBaseInfo& info) const;
    [[nodiscard]] bool updateUserBaseInfo(const UserBaseInfo & info) const;
    bool getUserPassword(UserBaseInfo& info) const;

    bool selectUserProfileInfo(int uid, UserProfile& info) const;
    bool selectUserFullInfo(UserBaseInfo& base, UserProfile& profile) const;
    bool updateUserProfileInfo(const UserProfile & profile) const;

private:
    std::unique_ptr<MysqlPool> pool_;
};


#endif //IMSERVER_USERINFODAO_H