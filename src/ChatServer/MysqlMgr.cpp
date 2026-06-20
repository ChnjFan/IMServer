//
// Created by Fan on 2026/5/8.
//

#include "MysqlMgr.h"

MysqlMgr::~MysqlMgr() {
}

bool MysqlMgr::addFriendApply(const int &from, const int &to) {
    return db_.addFriendApply(from, to);
}

bool MysqlMgr::getApplyUserList(int uid, ApplyUserList &applyUserList) {
    return db_.getApplyUserList(uid, applyUserList, 0, 10);
}

bool MysqlMgr::updateFriendRelation(int authUid, int applyUid) {
    return db_.updateFriendRelation(authUid, applyUid);
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

bool MysqlMgr::getFriendList(int uid, FriendInfoList &friendList) {
    return db_.getFriendList(uid, friendList, 0, 10);
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


