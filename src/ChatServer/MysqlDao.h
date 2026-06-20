#ifndef IMSERVER_MYSQLDAO_H
#define IMSERVER_MYSQLDAO_H

#include "ChatLogicSystem.h"
#include "MysqlPool.h"
#include "const.h"

class MysqlDao {
public:
    MysqlDao();
    ~MysqlDao();

    bool addFriendApply(const int& from, const int& to);

    bool getApplyUserList(int uid, ApplyUserList & applyUserList, int start, int size);

    bool updateFriendRelation(int authUid, int applyUid);
    bool updateUserInfo(const UserInfo & user_info);
    bool getUserInfo(UserInfo &user_info);
    bool getUserProfileInfo(int uid, UserProfileInfo &user_profile_info);

    bool getFriendList(int uid, FriendInfoList &friendList, const int start, const int size);

    bool addConversation(int uid, int to, const std::string& convId, int convType);
    bool getConversation(int uid, ConversationList& convList);

    bool addHistoryMessage(const MessageInfo& message);


private:
    static std::string getSearchPart(const UserInfo& user_info);

    std::unique_ptr<MysqlPool> pool_;
};

#endif //IMSERVER_MYSQLDAO_H
