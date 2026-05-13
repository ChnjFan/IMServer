//
// Created by Fan on 2026/5/8.
//

#ifndef IMSERVER_MYSQLMGR_H
#define IMSERVER_MYSQLMGR_H

#include "Singleton.h"
#include "MysqlDao.h"

class MysqlMgr : public Singleton<MysqlMgr> {
public:
    ~MysqlMgr();
    int registerUser(const std::string& name, const std::string& email, const std::string& password);

    bool checkEmail(const std::string& user, const std::string& email);

    bool updatePasswd(const std::string& user, const std::string& passwd);

    bool checkPasswd(const std::string& email, const std::string& passwd, UserInfo& userInfo);

private:
    friend class Singleton<MysqlMgr>;

    MysqlMgr() = default;
    MysqlDao db_;
};


#endif //IMSERVER_MYSQLMGR_H