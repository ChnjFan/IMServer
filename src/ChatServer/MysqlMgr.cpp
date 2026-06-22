//
// Created by Fan on 2026/5/8.
//

#include "MysqlMgr.h"

MysqlMgr::~MysqlMgr() {
}

bool MysqlMgr::getApplyUserList(int uid, ApplyUserList &applyUserList) {
    return db_.getApplyUserList(uid, applyUserList, 0, 10);
}

bool MysqlMgr::updateUserInfo(const UserInfo &userInfo) {
    return db_.updateUserInfo(userInfo);
}

bool MysqlMgr::getUserInfo(UserInfo &user_info) {
    return db_.getUserInfo(user_info);
}

bool MysqlMgr::getUserFullInfo(UserFullInfo &userFullInfo) {
    if (!getUserInfo(userFullInfo.baseInfo)) {
        std::cout << "Not found user by uid " << userFullInfo.baseInfo.uid << std::endl;
        return false;
    }
    return db_.getUserProfileInfo(userFullInfo.baseInfo.uid, userFullInfo.profileInfo);
}

bool MysqlMgr::getFriendRelation(const int uid, const int friendId, FriendRelation &fr) const {
    return db_.getFriendRelation(uid, friendId, fr);
}

bool MysqlMgr::checkFriendRelation(const int uid, const int friendId) {
    return db_.checkFriendRelation(uid, friendId);
}

bool MysqlMgr::checkFriendApply(const int uid, const int friendId) {
    return db_.checkFriendApply(uid, friendId);
}

bool MysqlMgr::updateFriendApply(const int uid, const int friendId, const int status, const std::string &msg) {
    return db_.updateFriendApply(uid, friendId, status, msg);
}

bool MysqlMgr::getFriendApplyCount(const int uid, int &count) {
    return db_.getFriendReplyCount(uid, count);
}

bool MysqlMgr::getFriendApplyList(int uid, int sinceId, std::vector<FriendApplyInfo> &applyInfoList) {
    return db_.getFriendApplyList(uid, sinceId, applyInfoList);
}

bool MysqlMgr::updateFriendRelation(const FriendInfo &friendInfo) {
    return db_.updateFriendRelation(friendInfo);
}

bool MysqlMgr::createFriendRelation(const FriendInfo &friendInfo) {
    return db_.createFriendRelation(friendInfo);
}

bool MysqlMgr::getFriendList(int uid, int sinceId, std::vector<FriendInfo> &friendList) {
    return db_.getFriendList(uid, sinceId, friendList);
}

bool MysqlMgr::addConversation(int uid, int to, const std::string &convId, int convType) {
    return db_.addConversation(uid, to, convId, convType);
}

bool MysqlMgr::getConversation(int uid, ConversationList &convList) {
    return db_.getConversation(uid, convList);
}

bool MysqlMgr::addHistoryMessage(MessageInfo &message) {
    return db_.addHistoryMessage(message);
}


