//
// Created by Fan on 2026/5/8.
//

#include "MysqlMgr.h"

#include "db/mysql/dao/UserInfoDao.h"
#include "db/mysql/dao/FriendInfoDao.h"

bool MysqlMgr::selectUserBaseInfo(UserBaseInfo &info) const {
    return userDao_.selectUserBaseInfo(info);
}

bool MysqlMgr::selectUserPassword(UserBaseInfo &info) const {
    return userDao_.getUserPassword(info);
}

bool MysqlMgr::selectUserProfileInfo(const int uid, UserProfile &info) const {
    return userDao_.selectUserProfileInfo(uid, info);
}

bool MysqlMgr::selectUserFullInfo(UserBaseInfo &base, UserProfile &profile) const {
    return userDao_.selectUserFullInfo(base, profile);
}

bool MysqlMgr::updateUserBaseInfo(const UserBaseInfo &info) const {
    return userDao_.updateUserBaseInfo(info);
}

bool MysqlMgr::updateUserProfileInfo(const UserProfile &profile) const {
    return userDao_.updateUserProfileInfo(profile);
}

bool MysqlMgr::selectFriend(const int uid, const int friendId, FriendInfo &info) const {
    return friendDao_.selectFriend(uid, friendId, info);
}

std::vector<FriendInfo> MysqlMgr::selectFriendList(const int uid, const std::string& sinceTime) const {
    return friendDao_.selectFriendList(uid, sinceTime);
}

bool MysqlMgr::isFriendExist(const int uid, const int friendId) {
    if (int status = -1; friendDao_.selectFriendStatus(uid, friendId, status)) {
        return status == static_cast<int>(FriendStatus::FRIEND_PRESENT);
    }
    return false;
}

bool MysqlMgr::createFriendRelation(const FriendApply &applyInfo) {
    return friendDao_.createFriendRelation(applyInfo);
}

bool MysqlMgr::updateFriendRelation(const int uid, const FriendInfo &friendInfo) {
    return friendDao_.updateFriendRelation(uid, friendInfo);
}

std::vector<FriendApply> MysqlMgr::selectFriendApplyList(const int uid, const std::string& sinceTime) const {
    return friendDao_.selectFriendApplyList(uid, sinceTime);
}

bool MysqlMgr::checkFriendApplyExist(const int uid, const int friendId) const {
    return friendDao_.checkFriendApplyExist(uid, friendId);
}

bool MysqlMgr::updateFriendApply(const int uid, const int friendId, const int status, const std::string &msg) {
    return friendDao_.updateFriendApply(uid, friendId, status, msg);
}

bool MysqlMgr::getFriendApplyCount(const int uid, int &count) {
    return friendDao_.getFriendApplyCount(uid, count);
}

bool MysqlMgr::createConversation(const ConversationInfo &info, std::string& result) const {
    return convDao_.createConversation(info, result);
}

std::vector<ConversationInfo> MysqlMgr::selectConversationList(const int uid, const std::string &sinceTime) {
    return convDao_.selectConversationList(uid, sinceTime);
}

bool MysqlMgr::createMessage(const MessageInfo &info, int &result) const {
    return convDao_.createMessage(info, result);
}

bool MysqlMgr::updateMessageStatus(const int id, const MessageStatus status) const {
    return convDao_.updateMessageStatus(id, status);
}

std::vector<MessageInfo> MysqlMgr::selectMessageList(const std::string &convId, const int sinceMsgId, const int limit) {
    return convDao_.selectMessageList(convId, sinceMsgId, limit);
}

bool MysqlMgr::updateConvMessagesStatus(const MessageStatusInfo &info) {
    return convDao_.updateConvMessagesStatus(info);
}






bool MysqlMgr::batchCreateMessages(const std::vector<std::shared_ptr<ChatMsgNode>>& nodes,
                                   std::unordered_map<int64_t, int>& id_mapping) {
    return convDao_.batchCreateMessages(nodes, id_mapping);
}
