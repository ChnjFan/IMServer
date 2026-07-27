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

int MysqlDao::registerUser(const std::string &user, const std::string &email, const std::string &password) const {
    auto conn = pool_->getConnect();
    if (!conn) {
        return -1;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });
    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement("CALL reg_user(?,?,?,?,@result)"));
        stmt->setString(1, user);
        stmt->setString(2, email);
        stmt->setString(3, password);
        stmt->setString(4, "");

        stmt->execute();
        

        const std::unique_ptr<sql::Statement> stmtResult(conn->conn_->createStatement());
        if (const std::unique_ptr<sql::ResultSet> res(stmtResult->executeQuery("SELECT @result AS result"));
                res->next()) {
            const int result = res->getInt("result");
            stmtResult->close();
            std::cout << "Register " << user << " Result: " << result << std::endl;
            return result;
        }
        stmtResult->close();
        return -1;
    } catch (sql::SQLException &e) {
        std::cout << "register user SQLException: " << e.what() << std::endl;
        return -1;
    }
}

bool MysqlDao::checkEmail(const std::string &email) const {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });
    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(
            conn->conn_->prepareStatement("SELECT name FROM user WHERE email = ?"));
        stmt->setString(1, email);
        const std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        
        while (res->next()) {
            std::cout << "Check name: " << res->getString("name") << std::endl;
            if (res->getString("name")->empty()) {
                return false;
            }
            return true;
        }
        std::cout << "Not foun email: " << email << std::endl;
        return false;
    } catch (sql::SQLException &e) {
        std::cout << "check email SQLException: " << e.what() << std::endl;
        return false;
    }
}

bool MysqlDao::updatePasswd(const std::string &email, const std::string &passwd) const {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });
    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(
            conn->conn_->prepareStatement("UPDATE user SET pwd = ? WHERE email = ?"));
        stmt->setString(1, passwd);
        stmt->setString(2, email);
        stmt->executeUpdate();
        return true;
    } catch (sql::SQLException &e) {
        std::cout << "check email SQLException: " << e.what() << std::endl;
        return false;
    }
}

bool MysqlDao::checkPasswd(const std::string &email, const std::string &passwd, UserInfo& userInfo) const {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });
    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(
            conn->conn_->prepareStatement("SELECT * FROM user WHERE email = ?"));
        stmt->setString(1, email);
        const std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        std::string originPassword;
        while (res->next()) {
            originPassword = res->getString("pwd");
            break;
        }
        if (originPassword != passwd) {
            std::cout << "Input passwd error." << std::endl;
            return false;
        }

        userInfo.uid = res->getInt("uid");
        userInfo.name = res->getString("name");
        userInfo.avatarUrl = res->getString("avatar_url");
        userInfo.password = originPassword;
        userInfo.email = email;
        return true;
    } catch (sql::SQLException &e) {
        std::cout << "check passwd SQLException: " << e.what() << std::endl;
        std::cout << "SQL error code: " << e.getErrorCode() << std::endl;
        std::cout << "SQL state: " << e.getSQLState() << std::endl;
        return false;
    }
}

int MysqlDao::getUid(const std::string &email) const {
    auto conn = pool_->getConnect();
    if (!conn) {
        return -1;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });
    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(
            conn->conn_->prepareStatement("SELECT uid FROM user WHERE email = ?"));
        stmt->setString(1, email);
        const std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        int uid = -1;
        while (res->next()) {
            uid = res->getInt("uid");
            break;
        }
        return uid;
    } catch (sql::SQLException &e) {
        std::cout << "check passwd SQLException: " << e.what() << std::endl;
        std::cout << "SQL error code: " << e.getErrorCode() << std::endl;
        std::cout << "SQL state: " << e.getSQLState() << std::endl;
        return -1;
    }
}
