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
#include "common/model/MessageInfo.h"
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
    [[nodiscard]] std::vector<FriendApply> selectFriendApplyList(int uid, const std::string& sinceTime) const;
    // 检查是否存在好友申请
    [[nodiscard]] bool checkFriendApplyExist(int uid, int friendId) const;
    bool updateFriendApply(int uid, int friendId, int status = 0, const std::string& msg = "");
    bool getFriendApplyCount(int uid, int& count);

    // 聊天会话
    bool createConversation(const ConversationInfo& info, std::string& result) const;
    std::vector<ConversationInfo> selectConversationList(int uid, const std::string& sinceTime);

    // 聊天消息
    bool createMessage(const MessageInfo& info, int& result) const;
    bool updateMessageStatus(int id, MessageStatus status) const;


private:
    friend class Singleton<MysqlMgr>;
    MysqlMgr() = default;

    UserInfoDao userDao_;
    FriendInfoDao friendDao_;
    ConversationDao convDao_;
};

#endif //IMSERVER_MYSQLMGR_H
