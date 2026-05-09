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
private:
    friend class Singleton<MysqlMgr>;

    MysqlMgr() = default;
    MysqlDao db_;
};


#endif //IMSERVER_MYSQLMGR_H