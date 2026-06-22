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

    bool getApplyUserList(int uid, ApplyUserList &applyUserList);

    bool updateUserInfo(const UserInfo& userInfo);
    bool getUserInfo(UserInfo& user_info);
    bool getUserFullInfo(UserFullInfo& userFullInfo);

    bool getFriendRelation(int uid, int friendId, FriendRelation & fr) const;
    bool checkFriendRelation(int uid, int friendId);

    bool checkFriendApply(int uid, int friendId);
    bool updateFriendApply(int uid, int friendId, int status, const std::string& msg);
    bool getFriendApplyCount(int uid, int &count);
    bool getFriendApplyList(int uid, int sinceId, std::vector<FriendApplyInfo> &applyInfoList);

    bool updateFriendRelation(const FriendInfo& friendInfo);
    bool createFriendRelation(const FriendInfo& friendInfo);

    bool getFriendList(int uid, int sinceId, std::vector<FriendInfo> &friendList);

    bool addConversation(int uid, int to, const std::string& convId, int convType);
    bool getConversation(int uid, ConversationList& convList);

    bool addHistoryMessage(MessageInfo& message);


private:
    friend class Singleton<MysqlMgr>;

    MysqlMgr() = default;
    MysqlDao db_;
};

#endif //IMSERVER_MYSQLMGR_H
