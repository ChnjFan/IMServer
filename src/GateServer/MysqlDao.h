#ifndef IMSERVER_MYSQLDAO_H
#define IMSERVER_MYSQLDAO_H

#include "MysqlPool.h"

class MysqlDao {
public:
    MysqlDao();
    ~MysqlDao();

    [[nodiscard]] int registerUser(const std::string& user, const std::string& email, const std::string& password) const;

    [[nodiscard]] bool checkEmail(const std::string & email) const;

    [[nodiscard]] bool updatePasswd(const std::string & user, const std::string & passwd) const;

    [[nodiscard]] bool checkPasswd(const std::string & email, const std::string & passwd, UserInfo& userInfo) const;

private:
    std::unique_ptr<MysqlPool> pool_;
};

#endif //IMSERVER_MYSQLDAO_H
