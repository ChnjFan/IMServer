#ifndef IMSERVER_MYSQLDAO_H
#define IMSERVER_MYSQLDAO_H

#include "MysqlPool.h"
#include "const.h"

class MysqlDao {
public:
    MysqlDao();
    ~MysqlDao();

    std::shared_ptr<UserInfo> getUser(int uid) const;

private:
    std::unique_ptr<MysqlPool> pool_;
};

#endif //IMSERVER_MYSQLDAO_H
