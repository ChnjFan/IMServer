//
// Created by Fan on 2026/5/8.
//

#include "MysqlMgr.h"

MysqlMgr::~MysqlMgr() {
}

std::shared_ptr<UserInfo> MysqlMgr::getUser(int uid) {
    return db_.getUser(uid);
}

std::shared_ptr<UserInfo> MysqlMgr::getUser(const std::string &name) {
    return db_.getUser(name);
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


