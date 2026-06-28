//
// Created by Fan on 2026/5/8.
//

#ifndef IMSERVER_MYSQLMGR_H
#define IMSERVER_MYSQLMGR_H

#include <vector>
#include <json/config.h>

#include "Singleton.h"
#include "const.h"

#include "common/model/UserBaseInfo.h"
#include "common/model/UserProfile.h"
#include "common/model/FriendApply.h"
#include "common/model/ConversationInfo.h"
#include "db/mysql/dao/UserInfoDao.h"
#include "db/mysql/dao/FriendInfoDao.h"
#include "db/mysql/dao/ConversationDao.h"

class MysqlMgr : public Singleton<MysqlMgr> {
public:
    ~MysqlMgr() = default;

    // 用户信息接口
    bool selectUserBaseInfo(UserBaseInfo& info) const;
    bool selectUserPassword(UserBaseInfo& info) const;
    // 详细信息必须用 UID 查询
    bool selectUserProfileInfo(int uid, UserProfile &info) const;
    bool selectUserFullInfo(UserBaseInfo& base, UserProfile& profile) const;
    // 修改用户信息
    bool updateUserBaseInfo(const UserBaseInfo& info) const;
    bool updateUserProfileInfo(const UserProfile& profile) const;

    // 好友信息接口
    [[nodiscard]] std::vector<FriendInfo> selectFriendList(int uid, const std::string& sinceTime) const;
    bool isFriendExist(int uid, int friendId);
    // 创建好友关系
    bool createFriendRelation(const FriendApply& applyInfo);
    bool updateFriendRelation(int uid, const FriendInfo& friendInfo);

    // 获取好友申请列表
    [[nodiscard]] std::vector<FriendApply> selectFriendApplyList(int uid, const std::string& sinceId) const;
    // 检查是否存在好友申请
    [[nodiscard]] bool checkFriendApplyExist(int uid, int friendId) const;
    bool updateFriendApply(int uid, int friendId, int status = 0, const std::string& msg = "");
    bool getFriendApplyCount(int uid, int& count);

    bool createConversation(const ConversationInfo& info, std::string& result);

    // ============= 旧接口 ==============
    // bool getApplyUserList(int uid, ApplyUserList &applyUserList);
    //
    // bool updateUserInfo(const UserInfo& userInfo);
    // bool updateUserInfo(const Json::Value &root, const PartsList& parts);
    //
    // bool getUserBaseInfo(UserInfo& info);
    // bool getUserFullInfo(UserFullInfo& userFullInfo);
    //
    // bool getFriendRelation(int uid, int friendId, FriendRelation & fr) const;
    // bool checkFriendRelation(int uid, int friendId);
    //
    // bool checkFriendApply(int uid, int friendId);
    // bool updateFriendApply(int uid, int friendId, int status, const std::string& msg);
    // bool getFriendApplyCount(int uid, int &count);
    // bool getFriendApplyList(int uid, int sinceId, std::vector<FriendApplyInfo> &applyInfoList);
    //
    // bool updateFriendRelation(const FriendInfo& friendInfo);
    // bool createFriendRelation(const FriendInfo& friendInfo);
    //
    // bool getFriendList(int uid, int sinceId, std::vector<FriendInfo> &friendList);
    //
    // bool addConversation(int uid, int to, const std::string& convId, int convType);
    // bool getConversation(int uid, ConversationList& convList);
    //
    // bool addHistoryMessage(MessageInfo& message);

private:
    friend class Singleton<MysqlMgr>;
    MysqlMgr() = default;

    UserInfoDao userDao_;
    FriendInfoDao friendDao_;
    ConversationDao convDao_;
    // MysqlDao db_;
};

#endif //IMSERVER_MYSQLMGR_H
