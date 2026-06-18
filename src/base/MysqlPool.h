#ifndef IMSERVER_MYSQLPOOL_H
#define IMSERVER_MYSQLPOOL_H

#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>

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
    std::mutex checkMtx_;
    std::condition_variable cond_;
    std::condition_variable checkCond_;
    std::atomic<bool> stop_{false};
    std::thread thread_;
};

#endif //IMSERVER_MYSQLPOOL_H
