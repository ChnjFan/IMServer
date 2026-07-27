//
// Created by Fan on 2026/6/24.
//

#ifndef IMSERVER_FRIENDINFODAO_H
#define IMSERVER_FRIENDINFODAO_H

#include "MysqlPool.h"
#include "common/model/FriendInfo.h"
#include "common/model/FriendApply.h"

class FriendInfoDao {
public:
    FriendInfoDao();
    ~FriendInfoDao();

    bool selectFriend(int uid, int friendId, FriendInfo& info) const;
    [[nodiscard]] std::vector<FriendInfo> selectFriendList(int uid, const std::string& sinceTime) const;
    bool selectFriendStatus(int uid, int friendId, int& status);

    [[nodiscard]] std::vector<FriendApply> selectFriendApplyList(int uid, const std::string& sinceTime) const;
    [[nodiscard]] bool checkFriendApplyExist(int uid, int friendId) const;
    bool updateFriendApply(int uid, int friendId, int status, const std::string & msg);
    bool getFriendApplyCount(int uid, int& count);

    bool createFriendRelation(const FriendApply & applyInfo);
    bool updateFriendRelation(int uid, const FriendInfo& friendInfo);

private:
    std::unique_ptr<MysqlPool> pool_;
};


#endif //IMSERVER_FRIENDINFODAO_H
