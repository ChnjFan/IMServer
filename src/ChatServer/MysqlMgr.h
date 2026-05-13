//
// Created by Fan on 2026/5/8.
//

#ifndef IMSERVER_MYSQLMGR_H
#define IMSERVER_MYSQLMGR_H

#include "Singleton.h"
#include "MysqlDao.h"
#include "const.h"

class MysqlMgr : public Singleton<MysqlMgr> {
public:
    ~MysqlMgr();

    std::shared_ptr<UserInfo> getUser(int uid);

private:
    friend class Singleton<MysqlMgr>;

    MysqlMgr() = default;
    MysqlDao db_;
};

#endif //IMSERVER_MYSQLMGR_H
