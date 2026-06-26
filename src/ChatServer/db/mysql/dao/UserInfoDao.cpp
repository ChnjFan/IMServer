//
// Created by Fan on 2026/6/23.
//

#include <format>
#include "UserInfoDao.h"
#include "ConfigMgr.h"

constexpr std::string_view USER_BASE_INFO_PARTS = "id, uid, gender, name, email, avatar, create_time, update_time";
constexpr std::string_view USER_PROFILE_INFO_PARTS = "uid, signature, birthday, region, self_intro, create_time";
constexpr std::string_view USER_PROFILE_CONFIG_PARTS = "uid, privacy_friend, privacy_chat, blacklist_switch";

UserInfoDao::UserInfoDao() {
    auto& conf = ConfigMgr::getInstance();
    const auto& host = conf["Mysql"]["Host"];
    const auto& port = conf["Mysql"]["Port"];
    const auto& user = conf["Mysql"]["User"];
    const auto& password = conf["Mysql"]["Password"];
    const auto& schema = conf["Mysql"]["Schema"];
    pool_ = std::make_unique<MysqlPool>(host + ":" + port, user, password, schema);
}

UserInfoDao::~UserInfoDao() {
    pool_->close();
}

std::vector<UserBaseInfo> UserInfoDao::selectUserListInfo(const UserBaseInfo &searchInfo) const {
    auto conn = pool_->getConnect();
    if (!conn) {
        return {};
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });
    try {
        std::vector<UserBaseInfo> result;
        const std::string sql = "SELECT " + std::string(USER_BASE_INFO_PARTS)
            + " FROM user WHERE " + searchInfo.getSearchProperty() + " = ?";
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(sql));
        if (const std::string value = searchInfo.getSearchPropertyStringValue(); !value.empty()) {
            stmt->setString(1, value);
        }
        else if (const int uid = searchInfo.getSearchPropertyIntValue(); uid >= 0) {
            stmt->setInt(1, uid);
        }
        else {
            return {};
        }

        const std::shared_ptr<sql::ResultSet> res(stmt->executeQuery());
        while (res->next()) {
            result.push_back(UserBaseInfo::fromResult(res));
        }
        return result;
    } catch (sql::SQLException& e) {
        std::cout << "get user info SQLException: " << e.what() << std::endl;
        return {};
    }
}

bool UserInfoDao::selectUserBaseInfo(UserBaseInfo &info) const {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });
    try {
        const std::string sql = "SELECT " + std::string(USER_BASE_INFO_PARTS)
            + " FROM user WHERE " + info.getSearchProperty() + " = ? LIMIT 1";
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(sql));
        if (const int uid = info.getSearchPropertyIntValue(); uid >= 0) {
            stmt->setInt(1, uid);
        }
        else if (const std::string value = info.getSearchPropertyStringValue(); !value.empty()) {
            stmt->setString(1, value);
        }
        else {
            return false;
        }

        if (const std::shared_ptr<sql::ResultSet> res(stmt->executeQuery()); res->next()) {
            info.fromSqlResult(res);
            return true;
        }
        return false;
    } catch (sql::SQLException& e) {
        std::cout << "get user info SQLException: " << e.what() << std::endl;
        return false;
    }
}

bool UserInfoDao::getUserPassword(UserBaseInfo &info) const {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });
    try {
        const std::string sql = "SELECT pwd, salt FROM user WHERE " + info.getSearchProperty() + " = ? LIMIT 1";
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(sql));
        if (const std::string value = info.getSearchPropertyStringValue(); !value.empty()) {
            stmt->setString(1, value);
        }
        else if (const int uid = info.getSearchPropertyIntValue(); uid >= 0) {
            stmt->setInt(1, uid);
        }
        else {
            return false;
        }

        if (const std::shared_ptr<sql::ResultSet> res(stmt->executeQuery()); res->next()) {
            info.fromSqlResult(res);
            return true;
        }
        return false;
    } catch (sql::SQLException& e) {
        std::cout << "get user info SQLException: " << e.what() << std::endl;
        return false;
    }
}

bool UserInfoDao::selectUserProfileInfo(const int uid, UserProfile &info) const {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });

    if (uid < 0 ) {
        std::cout << "[selectUserProfileInfo] Invalid param uid: " << uid << std::endl;
        return false;
    }

    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(
            conn->conn_->prepareStatement("SELECT * FROM user_profile WHERE uid = ?"));
        stmt->setInt(1, uid);

        if (const std::shared_ptr<sql::ResultSet> res(stmt->executeQuery()); res->next()) {
            info.fromSqlResult(res);
            return true;
        }
        return false;
    } catch (sql::SQLException& e) {
        std::cout << "get user info SQLException: " << e.what() << std::endl;
        return false;
    }
}

bool UserInfoDao::selectUserFullInfo(UserBaseInfo &base, UserProfile &profile) const {
    selectUserBaseInfo(base);
    return selectUserProfileInfo(base.uid, profile);
}
