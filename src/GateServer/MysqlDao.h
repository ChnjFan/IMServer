#ifndef IMSERVER_MYSQLDAO_H
#define IMSERVER_MYSQLDAO_H

#include "MysqlPool.h"

class MysqlDao {
public:
    MysqlDao();
    ~MysqlDao();

    int registerUser(const std::string& user, const std::string& email, const std::string& password);

    bool checkEmail(const std::string & user, const std::string & email);

    bool updatePasswd(const std::string & user, const std::string & passwd);

    bool checkPasswd(const std::string & email, const std::string & passwd, UserInfo& userInfo);

private:
    std::unique_ptr<MysqlPool> pool_;
};

#endif //IMSERVER_MYSQLDAO_H
