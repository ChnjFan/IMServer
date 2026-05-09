//
// Created by Fan on 2026/5/8.
//

#include "MysqlDao.h"

#include <iostream>
#include <chrono>

#include "ConfigMgr.h"

SqlConnection::SqlConnection(sql::Connection *conn, int64_t lastTime) : conn_(conn), lastOptTime_(lastTime) {
}

MysqlPool::MysqlPool(const std::string &url, const std::string &user,
    const std::string &password, const std::string &schema, const int size)
    : url_(url), user_(user), password_(password), schema_(schema), poolSize_(size) {
    try {
        std::cout << "Create MysqlPool with " << user << "....";
        for (int i = 0; i < size; i++) {
            const auto driver = sql::mysql::get_driver_instance();
            if (!driver) {
                throw std::runtime_error("Driver is null");
            }

            const auto conn = driver->connect(url, user, password);
            if (!conn) {
                throw std::runtime_error("Connection is null");
            }

            conn->setSchema(schema_);
            // 获取当前时间戳转化成秒，设置连接操作的时间
            auto curTime = std::chrono::system_clock::now().time_since_epoch();
            long long timeStamp = std::chrono::duration_cast<std::chrono::seconds>(curTime).count();
            connections_.push(std::make_unique<SqlConnection>(conn, timeStamp));
        }

        thread_ = std::thread([&]() {
            while (!stop_.load()) {
                checkConnection();
                std::this_thread::sleep_for(std::chrono::seconds(60));
            }
        });
        thread_.detach();   // 分离线程
        std::cout << "OK" << std::endl;
    } catch (sql::SQLException &e) {
        std::cout << "SQLException: " << e.what() << std::endl;
    }
}

MysqlPool::~MysqlPool() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!connections_.empty()) {
        connections_.pop();
    }
}

std::unique_ptr<SqlConnection> MysqlPool::getConnect() {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock, [this]() {
        if (stop_.load()) {
            return true;
        }
        return !connections_.empty();
    });
    if (stop_.load()) {
        return nullptr;
    }
    std::unique_ptr<SqlConnection> conn(std::move(connections_.front()));
    connections_.pop();
    return conn;
}

void MysqlPool::returnConnect(std::unique_ptr<SqlConnection> conn) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stop_.load()) {
        return;
    }
    connections_.push(std::move(conn));
    cond_.notify_one(); // 通知一个线程获取连接
}

void MysqlPool::close() {
    stop_.store(true);
    cond_.notify_all();
}

void MysqlPool::checkConnection() {
    std::lock_guard<std::mutex> guard(mutex_);
    auto poolSize = connections_.size();
    // 更新操作时间
    const auto curTime = std::chrono::system_clock::now().time_since_epoch();
    const long long timeStamp = std::chrono::duration_cast<std::chrono::seconds>(curTime).count();
    for (int i = 0; i < poolSize_; i++) {
        auto conn = std::move(connections_.front());
        connections_.pop();
        // 出作用域析构时候再放回到连接池
        Defer defer([this, &conn]() {
            connections_.push(std::move(conn));
        });

        if (timeStamp - conn->lastOptTime_ < 60) { // 1分钟没有操作，重新操作更新时间
            continue;
        }

        try {
            // 执行 SQL 更新操作
            std::unique_ptr<sql::Statement> stmt(conn->conn_->createStatement());
            stmt->executeQuery("SELECT 1");// 最简单的连通性测试
            conn->lastOptTime_ = timeStamp;
            std::cout << "Execute aliver query, timestamp: " << timeStamp << std::endl;
        } catch (sql::SQLException &e) {
            std::cout << "Error connection alive, SQLException: " << e.what() << std::endl;
            // 重新连接
            const auto driver = sql::mysql::get_driver_instance();
            if (!driver) {
                return; // 连接失败下次重试
            }
            const auto newConn = driver->connect(url_, user_, password_);
            if (!newConn) {
                return; // 连接失败下次重试
            }
            newConn->setSchema(schema_);
            conn->conn_.reset(newConn);
            conn->lastOptTime_ = timeStamp;
        }
    }
}

MysqlDao::MysqlDao() {
    auto& conf = ConfigMgr::getInstance();
    const auto& host = conf["Mysql"]["Host"];
    const auto& port = conf["Mysql"]["Port"];
    const auto& user = conf["Mysql"]["User"];
    const auto& password = conf["Mysql"]["Password"];
    const auto& schema = conf["Mysql"]["Schema"];
    pool_ = std::make_unique<MysqlPool>(host + ":" + port, user, password, schema);
}

MysqlDao::~MysqlDao() {
    pool_->close();
}

int MysqlDao::registerUser(const std::string &user, const std::string &email, const std::string &password) {
    auto conn = pool_->getConnect();
    if (!conn) {
        return -1;
    }
    try {
        // 带参数的 SQL 语句要使用 prepareStatement 防止 SQL 注入
        // CALL 语句调用一个 SQL 函数，存储过程，会话变量 @reuslt 保存输出结果
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement("CALL reg_user(?,?,?,@result)"));
        stmt->setString(1, user);
        stmt->setString(2, email);
        stmt->setString(3, password);

        stmt->execute();

        // prepareStatement 不支持注册输出参数，要用其他会话变量获取输出参数的值
        const std::unique_ptr<sql::Statement> stmtResult(conn->conn_->createStatement());
        if (const std::unique_ptr<sql::ResultSet> res(stmtResult->executeQuery("SELECT @result AS result"));
                res->next()) {
            const int result = res->getInt("result");
            std::cout << "Result: " << result << std::endl;
            pool_->returnConnect(std::move(conn));
            return result;
        }

        pool_->returnConnect(std::move(conn));
        return -1;
    } catch (sql::SQLException &e) {
        std::cout << "register user SQLException: " << e.what() << std::endl;
        return -1;
    }
}
