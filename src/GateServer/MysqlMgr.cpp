//
// Created by Fan on 2026/5/8.
//

#include "MysqlMgr.h"

MysqlMgr::~MysqlMgr() {
}

int MysqlMgr::registerUser(const std::string &name, const std::string &email, const std::string &password) {
    return db_.registerUser(name, email, password);
}

bool MysqlMgr::checkEmail(const std::string &email) {
    return db_.checkEmail(email);
}

bool MysqlMgr::updatePasswd(const std::string &email, const std::string &passwd) {
    return db_.updatePasswd(email, passwd);
}

bool MysqlMgr::checkPasswd(const std::string &email, const std::string &passwd, UserInfo &userInfo) {
    return db_.checkPasswd(email, passwd, userInfo);
}

