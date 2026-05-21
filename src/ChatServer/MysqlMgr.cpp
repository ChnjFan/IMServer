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


