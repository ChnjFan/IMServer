#ifndef IMSERVER_MYSQLDAO_H
#define IMSERVER_MYSQLDAO_H

#include "MysqlPool.h"
#include "const.h"
#include "FriendRelation.h"

class MysqlDao {
public:
    MysqlDao();
    ~MysqlDao();

    bool getApplyUserList(int uid, ApplyUserList & applyUserList, int start, int size);

    bool updateUserInfo(const UserInfo & user_info);
    bool updateUserInfo(const Json::Value& root, const PartsList& parts);
    bool getUserInfo(UserInfo &user_info);
    bool getUserProfileInfo(int uid, UserProfileInfo &user_profile_info);

    bool getFriendRelation(int uid, int friendId, FriendRelation & fr) const;
    [[nodiscard]] bool checkFriendRelation(int uid, int friendId) const;

    [[nodiscard]] bool checkFriendApply(int uid, int friendId) const;
    [[nodiscard]] bool updateFriendApply(int uid, int friendId, int status, const std::string& msg) const;
    bool getFriendReplyCount(int uid, int& count);
    bool getFriendApplyList(int uid, int since_id, std::vector<FriendApplyInfo> & applyList);

    bool updateFriendRelation(const FriendInfo &friendInfo);
    bool createFriendRelation(const FriendInfo &friendInfo);

    bool getFriendList(int uid, int sinceId, std::vector<FriendInfo> &friendList);

    bool addConversation(int uid, int to, const std::string& convId, int convType);
    bool getConversation(int uid, ConversationList& convList);

    bool addHistoryMessage(const MessageInfo& message);

private:
    static std::string getSearchPart(const UserInfo& user_info);

    std::unique_ptr<MysqlPool> pool_;
};

#endif //IMSERVER_MYSQLDAO_H
