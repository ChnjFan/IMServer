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

    bool addFriendApply(const int& from, const int& to);

    bool getApplyUserList(int uid, ApplyUserList &applyUserList);

    bool updateFriendRelation(int authUid, int applyUid);

    bool updateUserInfo(const UserInfo& userInfo);
    bool getUserInfo(UserInfo& user_info);
    bool getUserFullInfo(UserFullInfo& userFullInfo);

    bool getFriendList(int uid, FriendInfoList &friendList);

    bool addConversation(int uid, int to, const std::string& convId, int convType);
    bool getConversation(int uid, ConversationList& convList);

    bool addHistoryMessage(MessageInfo& message);


private:
    friend class Singleton<MysqlMgr>;

    MysqlMgr() = default;
    MysqlDao db_;
};

#endif //IMSERVER_MYSQLMGR_H
