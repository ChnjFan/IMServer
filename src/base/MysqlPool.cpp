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
                checkConnection();
                std::this_thread::sleep_for(std::chrono::seconds(60));
            }
        });
        thread_.detach();
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
    cond_.notify_one();
}

void MysqlPool::close() {
    stop_.store(true);
    cond_.notify_all();
}

void MysqlPool::checkConnection() {
    std::lock_guard<std::mutex> guard(mutex_);
    auto poolSize = connections_.size();
    const auto curTime = std::chrono::system_clock::now().time_since_epoch();
    const long long timeStamp = std::chrono::duration_cast<std::chrono::seconds>(curTime).count();
    for (int i = 0; i < poolSize_; i++) {
        auto conn = std::move(connections_.front());
        connections_.pop();
        Defer defer([this, &conn]() {
            connections_.push(std::move(conn));
        });

        if (timeStamp - conn->lastOptTime_ < 60) {
            continue;
        }

        try {
            std::unique_ptr<sql::Statement> stmt(conn->conn_->createStatement());
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
