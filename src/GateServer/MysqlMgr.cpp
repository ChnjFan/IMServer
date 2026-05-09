//
// Created by Fan on 2026/5/8.
//

#include "MysqlMgr.h"

MysqlMgr::~MysqlMgr() {
}

int MysqlMgr::registerUser(const std::string &name, const std::string &email, const std::string &password) {
    return db_.registerUser(name, email, password);
}

bool MysqlMgr::checkEmail(const std::string &user, const std::string &email) {
    return db_.checkEmail(user, email);
}

bool MysqlMgr::updatePasswd(const std::string &user, const std::string &passwd) {
    return db_.updatePasswd(user, passwd);
}

