#include "MysqlPool.h"

#include <iostream>
#include <chrono>

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
            auto curTime = std::chrono::system_clock::now().time_since_epoch();
            long long timeStamp = std::chrono::duration_cast<std::chrono::seconds>(curTime).count();
            connections_.push(std::make_unique<SqlConnection>(conn, timeStamp));
        }

        thread_ = std::thread([&]() {
            while (!stop_.load()) {
                try {
                    std::unique_lock<std::mutex> lock(checkMtx_);
                    checkCond_.wait_for(lock, std::chrono::seconds(60), [this]() {
                        return stop_.load();
                    });
                    if (stop_.load()) {
                        break;
                    }
                    checkConnection();
                } catch (std::exception &e) {
                    std::cout << e.what() << std::endl;
                }
            }
        });
        std::cout << "OK" << std::endl;
    } catch (sql::SQLException &e) {
        std::cout << "SQLException: " << e.what() << std::endl;
    }
}

MysqlPool::~MysqlPool() {
    stop_.store(true);
    checkCond_.notify_all();  // 唤醒正在 wait 的健康检查线程
    if (thread_.joinable()) {
        thread_.join();
    }
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
    cond_.notify_one();
}

void MysqlPool::close() {
    stop_.store(true);
    cond_.notify_all();
    checkCond_.notify_all();  // 唤醒健康检查线程
}

void MysqlPool::checkConnection() {
    std::lock_guard<std::mutex> guard(mutex_);
    const auto poolSize = connections_.size();
    const auto curTime = std::chrono::system_clock::now().time_since_epoch();
    const long long timeStamp = std::chrono::duration_cast<std::chrono::seconds>(curTime).count();
    for (int i = 0; i < poolSize; i++) {
        auto conn = std::move(connections_.front());
        connections_.pop();
        Defer defer([this, &conn]() {
            connections_.push(std::move(conn));
        });

        if (timeStamp - conn->lastOptTime_ < 60) {
            continue;
        }

        try {
            const std::unique_ptr<sql::Statement> stmt(conn->conn_->createStatement());
            stmt->executeQuery("SELECT 1");
            conn->lastOptTime_ = timeStamp;
        } catch (sql::SQLException &e) {
            std::cout << "Error connection alive, SQLException: " << e.what() << std::endl;
            const auto driver = sql::mysql::get_driver_instance();
            if (!driver) {
                return;
            }
            const auto newConn = driver->connect(url_, user_, password_);
            if (!newConn) {
                return;
            }
            newConn->setSchema(schema_);
            conn->conn_.reset(newConn);
            conn->lastOptTime_ = timeStamp;
        }
    }
}
