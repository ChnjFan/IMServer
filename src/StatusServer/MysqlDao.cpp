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

int MysqlDao::registerUser(const std::string &user, const std::string &email, const std::string &password) {
    auto conn = pool_->getConnect();
    if (!conn) {
        return -1;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });
    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement("CALL reg_user(?,?,?,@result)"));
        stmt->setString(1, user);
        stmt->setString(2, email);
        stmt->setString(3, password);

        stmt->execute();

        const std::unique_ptr<sql::Statement> stmtResult(conn->conn_->createStatement());
        if (const std::unique_ptr<sql::ResultSet> res(stmtResult->executeQuery("SELECT @result AS result"));
                res->next()) {
            const int result = res->getInt("result");
            std::cout << "Result: " << result << std::endl;
            return result;
        }
        return -1;
    } catch (sql::SQLException &e) {
        std::cout << "register user SQLException: " << e.what() << std::endl;
        return -1;
    }
}

bool MysqlDao::checkEmail(const std::string &user, const std::string &email) {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });
    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(
            conn->conn_->prepareStatement("SELECT email FROM user WHERE name = ?"));
        stmt->setString(1, user);
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        while (res->next()) {
            std::cout << "Check email: " << res->getString("email") << std::endl;
            if (email != res->getString("email")) {
                return false;
            }
            return true;
        }
        return false;
    } catch (sql::SQLException &e) {
        std::cout << "check email SQLException: " << e.what() << std::endl;
        return false;
    }
}

bool MysqlDao::updatePasswd(const std::string &user, const std::string &passwd) {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });
    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(
            conn->conn_->prepareStatement("UPDATE user SET pwd = ? WHERE name = ?"));
        stmt->setString(1, passwd);
        stmt->setString(2, user);
        const auto res = (stmt->executeUpdate());
        std::cout << "Update rows: " << res << std::endl;
        return true;
    } catch (sql::SQLException &e) {
        std::cout << "check email SQLException: " << e.what() << std::endl;
        return false;
    }
}

bool MysqlDao::checkPasswd(const std::string &email, const std::string &passwd, UserInfo userInfo) {
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
        const auto res = (stmt->executeQuery());
        std::string originPassword;
        while (res->next()) {
            originPassword = res->getString("pwd");
            std::cout << "Get passwd: " << originPassword << std::endl;
            break;
        }
        if (originPassword != passwd) {
            std::cout << "Input passwd: " << passwd << std::endl;
            return false;
        }

        userInfo.uid = res->getInt("uid");
        userInfo.name = res->getString("name");
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
