#ifndef IMSERVER_MYSQLDAO_H
#define IMSERVER_MYSQLDAO_H

#include "ChatLogicSystem.h"
#include "MysqlPool.h"
#include "const.h"

class MysqlDao {
public:
    MysqlDao();
    ~MysqlDao();

    std::shared_ptr<UserInfo> getUser(int uid) const;
    std::shared_ptr<UserInfo> getUser(const std::string& name) const;

    bool addFriendApply(const int& from, const int& to);

    bool getApplyUserList(int uid, ApplyUserList & applyUserList, int start, int size);

    bool updateFriendRelation(int authUid, int applyUid);

    bool getFriendList(int uid, FriendInfoList &friendList, const int start, const int size);

private:
    std::unique_ptr<MysqlPool> pool_;
};

#endif //IMSERVER_MYSQLDAO_H
