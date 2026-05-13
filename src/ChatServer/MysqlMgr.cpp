//
// Created by Fan on 2026/5/8.
//

#include "MysqlMgr.h"

MysqlMgr::~MysqlMgr() {
}

std::shared_ptr<UserInfo> MysqlMgr::getUser(int uid) {
    return db_.getUser(uid);
}


