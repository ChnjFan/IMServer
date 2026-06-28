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

bool MysqlMgr::createConversation(const ConversationInfo &info, std::string& result) {
    return convDao_.createConversation(info, result);
}

//
// bool MysqlMgr::getApplyUserList(int uid, ApplyUserList &applyUserList) {
//     return db_.getApplyUserList(uid, applyUserList, 0, 10);
// }
//
// bool MysqlMgr::updateUserInfo(const UserInfo &userInfo) {
//     return db_.updateUserInfo(userInfo);
// }
//
// bool MysqlMgr::updateUserInfo(const Json::Value &root, const PartsList& parts) {
//     return db_.updateUserInfo(root, parts);
// }
//
// bool MysqlMgr::getUserBaseInfo(UserInfo &info) {
//     return db_.getUserBaseInfo(info);
// }
//
// bool MysqlMgr::getUserFullInfo(UserFullInfo &userFullInfo) {
//     if (!getUserBaseInfo(userFullInfo.baseInfo)) {
//         std::cout << "Not found user by uid " << userFullInfo.baseInfo.uid << std::endl;
//         return false;
//     }
//     return db_.getUserProfileInfo(userFullInfo.baseInfo.uid, userFullInfo.profileInfo);
// }
//
// bool MysqlMgr::getFriendRelation(const int uid, const int friendId, FriendRelation &fr) const {
//     return db_.getFriendRelation(uid, friendId, fr);
// }
//
// bool MysqlMgr::checkFriendRelation(const int uid, const int friendId) {
//     return db_.checkFriendRelation(uid, friendId);
// }
//
// bool MysqlMgr::checkFriendApply(const int uid, const int friendId) {
//     return db_.checkFriendApply(uid, friendId);
// }
//
// bool MysqlMgr::updateFriendApply(const int uid, const int friendId, const int status, const std::string &msg) {
//     return db_.updateFriendApply(uid, friendId, status, msg);
// }
//
// bool MysqlMgr::getFriendApplyCount(const int uid, int &count) {
//     return db_.getFriendReplyCount(uid, count);
// }
//
// bool MysqlMgr::getFriendApplyList(int uid, int sinceId, std::vector<FriendApplyInfo> &applyInfoList) {
//     return db_.getFriendApplyList(uid, sinceId, applyInfoList);
// }
//
// bool MysqlMgr::updateFriendRelation(const FriendInfo &friendInfo) {
//     return db_.updateFriendRelation(friendInfo);
// }
//
// bool MysqlMgr::createFriendRelation(const FriendInfo &friendInfo) {
//     return db_.createFriendRelation(friendInfo);
// }
//
// bool MysqlMgr::getFriendList(int uid, int sinceId, std::vector<FriendInfo> &friendList) {
//     return db_.getFriendList(uid, sinceId, friendList);
// }
//
// bool MysqlMgr::addConversation(int uid, int to, const std::string &convId, int convType) {
//     return db_.addConversation(uid, to, convId, convType);
// }
//
// bool MysqlMgr::getConversation(int uid, ConversationList &convList) {
//     return db_.getConversation(uid, convList);
// }
//
// bool MysqlMgr::addHistoryMessage(MessageInfo &message) {
//     return db_.addHistoryMessage(message);
// }



