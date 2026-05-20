#include "MysqlDao.h"

#include <iostream>

#include "ConfigMgr.h"
#include "const.h"

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

std::shared_ptr<UserInfo> MysqlDao::getUser(const int uid) const {
    auto conn = pool_->getConnect();
    if (!conn) {
        return nullptr;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });
    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement("SELECT * FROM user WHERE uid = ?"));
        stmt->setInt(1, uid);
        const std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        while (res->next()) {
            std::cout << "Get user: " << res->getInt("uid") << std::endl;
            if (uid != res->getInt("uid")) {
                return nullptr;
            }
            const auto user = std::make_shared<UserInfo>();
            user->uid = res->getInt("uid");
            user->email = res->getString("email");
            user->name = res->getString("name");
            return user;
        }
        return nullptr;
    } catch (sql::SQLException& e) {
        std::cout << "get user SQLException: " << e.what() << std::endl;
        return nullptr;
    }
}

std::shared_ptr<UserInfo> MysqlDao::getUser(const std::string &name) const {
    auto conn = pool_->getConnect();
    if (!conn) {
        return nullptr;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });
    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement("SELECT * FROM user WHERE name = ?"));
        stmt->setString(1, name);
        const std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        while (res->next()) {
            std::cout << "Get user: " << res->getInt("uid") << std::endl;
            if (name != res->getString("name")) {
                return nullptr;
            }
            const auto user = std::make_shared<UserInfo>();
            user->uid = res->getInt("uid");
            user->email = res->getString("email");
            user->name = res->getString("name");
            return user;
        }
        return nullptr;
    } catch (sql::SQLException& e) {
        std::cout << "get user SQLException: " << e.what() << std::endl;
        return nullptr;
    }
}

bool MysqlDao::addFriendApply(const int &from, const int &to) {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });

    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
            "INSERT INTO friend_apply (uid, friend_id) values (?,?) "
            "ON DUPLICATE KEY UPDATE uid = uid, friend_id = friend_id"));
        std::cout << "Insert friend_apply (" << from << ", " << to << ")" << std::endl;
        stmt->setInt(1, from);
        stmt->setInt(2, to);
        if (const int rowAffected = stmt->executeUpdate(); rowAffected < 0) {
            return false;
        }
        return true;
    } catch (sql::SQLException& e) {
        std::cout << "get user SQLException: " << e.what() << std::endl;
        return false;
    }
}

bool MysqlDao::getApplyUserList(const int uid, ApplyUserList &applyUserList, const int start, const int size) {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });

    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
            "SELECT apply.uid, apply.status, user.name, user.email "
                    "FROM friend_apply as apply join user on apply.uid = user.uid WHERE apply.friend_id = ? "
                    "AND apply.id > ? ORDER by apply.id ASC LIMIT ? "));
        stmt->setInt(1, uid);
        stmt->setInt(2, start);
        stmt->setInt(3, size);
        const std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        while (res->next()) {
            std::cout << "Get user: " << res->getInt("uid") << std::endl;
            auto user = std::make_shared<ApplyUserInfo>();
            user->uid = res->getInt("uid");
            user->status = res->getInt("status");
            user->email = res->getString("email");
            user->name = res->getString("name");
            applyUserList.push_back(std::move(user));
        }
        return true;
    } catch (sql::SQLException& e) {
        std::cout << "get user SQLException: " << e.what() << std::endl;
        return false;
    }
}

bool MysqlDao::updateFriendRelation(const int authUid, const int applyUid) {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });
    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement("CALL add_friend_relation(?,?,@result)"));
        stmt->setInt(1, authUid);
        stmt->setInt(2, applyUid);

        stmt->execute();

        const std::unique_ptr<sql::Statement> stmtResult(conn->conn_->createStatement());
        if (const std::unique_ptr<sql::ResultSet> res(stmtResult->executeQuery("SELECT @result AS result"));
                res->next()) {
            const int result = res->getInt("result");
            std::cout << "Result: " << result << std::endl;
            return (result == 0);
        }
        return false;
    } catch (sql::SQLException &e) {
        std::cout << "add friend relation SQLException: " << e.what() << std::endl;
        return false;
    }
}
