//
// Created by Fan on 2026/5/8.
//

#ifndef IMSERVER_MYSQLMGR_H
#define IMSERVER_MYSQLMGR_H

#include <json/config.h>

#include "ChatLogicSystem.h"
#include "Singleton.h"
#include "MysqlDao.h"
#include "const.h"

class MysqlMgr : public Singleton<MysqlMgr> {
public:
    ~MysqlMgr();

    std::shared_ptr<UserInfo> getUser(int uid);
    std::shared_ptr<UserInfo> getUser(const std::string &name);

    bool addFriendApply(const int& from, const int& to);

    bool getApplyUserList(int uid, ApplyUserList &applyUserList);

    bool updateFriendRelation(int authUid, int applyUid);

    bool getFriendList(int uid, FriendInfoList &friendList);


private:
    friend class Singleton<MysqlMgr>;

    MysqlMgr() = default;
    MysqlDao db_;
};

#endif //IMSERVER_MYSQLMGR_H
