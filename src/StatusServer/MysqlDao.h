//
// Created by Fan on 2026/5/8.
//

#ifndef IMSERVER_MYSQLDAO_H
#define IMSERVER_MYSQLDAO_H

#include <memory>

#include <jdbc/mysql_driver.h>
#include <jdbc/mysql_connection.h>
#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/cppconn/resultset.h>
#include <jdbc/cppconn/statement.h>
#include <jdbc/cppconn/exception.h>

#include "const.h"

class SqlConnection {
public:
    SqlConnection(sql::Connection* conn, int64_t lastTime);

    std::unique_ptr<sql::Connection> conn_;
    int64_t lastOptTime_;
};

class MysqlPool {
public:
    MysqlPool(const std::string& url, const std::string& user, const std::string& password,
        const std::string& schema, int size = DEFAULT_MYSQL_POOL_SIZE);
    ~MysqlPool();

    std::unique_ptr<SqlConnection> getConnect();
    void returnConnect(std::unique_ptr<SqlConnection> conn);

    void close();
private:
    void checkConnection();

    std::string url_;
    std::string user_;
    std::string password_;
    std::string schema_;
    int poolSize_ = DEFAULT_MYSQL_POOL_SIZE;

    std::queue<std::unique_ptr<SqlConnection>> connections_;
    std::mutex mutex_;
    std::condition_variable cond_;
    std::atomic<bool> stop_{false};
    std::thread thread_;    // 后台线程，每分钟检测 sql 连接是否正常
};

class MysqlDao {
public:
    MysqlDao();
    ~MysqlDao();

    int registerUser(const std::string& user, const std::string& email, const std::string& password);

    bool checkEmail(const std::string & user, const std::string & email);

    bool updatePasswd(const std::string & user, const std::string & passwd);

    bool checkPasswd(const std::string & email, const std::string & passwd, UserInfo userInfo);

private:
    std::unique_ptr<MysqlPool> pool_;
};


#endif //IMSERVER_MYSQLDAO_H