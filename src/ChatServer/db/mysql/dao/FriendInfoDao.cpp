//
// Created by Fan on 2026/6/24.
//

#include "FriendInfoDao.h"
#include "ConfigMgr.h"
#include "common/model/FriendApply.h"

constexpr std::string_view FRIEND_LIST_INFO_PARTS = "friend_relation.friend_id, friend_relation.update_time, "
                                                    "friend_relation.alias, friend_relation.status, "
                                                    "friend_relation.is_star, friend_relation.create_time, "
                                                    "user.name, user.avatar_url, user.email ";
constexpr std::string_view FRIEND_INFO_STATUS_PARTS = "uid, friend_id, status ";
constexpr std::string_view FRIEND_APPLY_INFO_STATUS_PARTS = "friend_apply.uid, friend_apply.friend_id, "
                                                    "friend_apply.msg, friend_apply.status, friend_apply.expire_time, "
                                                    "friend_apply.create_time, friend_apply.update_time, "
                                                    "user.name, user.email, user.gender ";

FriendInfoDao::FriendInfoDao() {
    auto& conf = ConfigMgr::getInstance();
    const auto& host = conf["Mysql"]["Host"];
    const auto& port = conf["Mysql"]["Port"];
    const auto& user = conf["Mysql"]["User"];
    const auto& password = conf["Mysql"]["Password"];
    const auto& schema = conf["Mysql"]["Schema"];
    pool_ = std::make_unique<MysqlPool>(host + ":" + port, user, password, schema);
}

FriendInfoDao::~FriendInfoDao() {
    pool_->close();
}

std::vector<FriendInfo> FriendInfoDao::selectFriendList(const int uid, const std::string& sinceTime) const {
    auto conn = pool_->getConnect();
    if (!conn) {
        return {};
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });

    try {
        std::vector<FriendInfo> result;
        const std::string sql = "SELECT " + std::string(FRIEND_LIST_INFO_PARTS)
                                + "FROM friend_relation JOIN user ON friend_relation.friend_id = user.uid "
                                "WHERE friend_relation.uid = ? AND friend_relation.is_hide = 0 "
                                "AND friend_relation.status != 3 AND friend_relation.update_time > ? "
                                "ORDER by friend_relation.update_time ASC LIMIT ? ";
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(sql));
        stmt->setInt(1, uid);
        stmt->setString(2, sinceTime);
        stmt->setInt(3, 200);
        const std::shared_ptr<sql::ResultSet> res(stmt->executeQuery());
        
        while (res->next()) {
            result.push_back(FriendInfo::fromFriendListSearch(res));
        }
        return result;
    } catch (sql::SQLException& e) {
        std::cout << "get user SQLException: " << e.what() << std::endl;
        return {};
    }
}

bool FriendInfoDao::selectFriendStatus(const int uid, const int friendId, int& status) {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });

    try {
        const std::string sql = "SELECT " + std::string(FRIEND_INFO_STATUS_PARTS)
                                + "FROM friend_relation "
                                "WHERE friend_relation.uid = ? AND friend_relation.friend_id = ? "
                                "AND friend_relation.is_hide = 0 ";
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(sql));
        stmt->setInt(1, uid);
        stmt->setInt(2, friendId);
        if (const std::shared_ptr<sql::ResultSet> res(stmt->executeQuery()); res->next()) {
            
            status = res->getInt("status");
            return true;
        }
        
        return false;
    } catch (sql::SQLException& e) {
        std::cout << "get user SQLException: " << e.what() << std::endl;
        return false;
    }
}

std::vector<FriendApply> FriendInfoDao::selectFriendApplyList(const int uid, const std::string& sinceTime) const {
    auto conn = pool_->getConnect();
    if (!conn) {
        return {};
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });

    try {
        std::vector<FriendApply> result;
        const std::string sql = "SELECT " + std::string(FRIEND_APPLY_INFO_STATUS_PARTS)
                                + "FROM friend_apply "
                                "INNER JOIN user "
                                "ON user.uid = CASE "
                                    "WHEN friend_apply.uid = ? THEN friend_apply.friend_id "
                                    "ELSE friend_apply.uid END "
                                "WHERE (friend_apply.uid = ? OR friend_apply.friend_id = ?) "
                                "AND friend_apply.update_time > ? "
                                "ORDER by friend_apply.id ASC LIMIT ? ";
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(sql));
        stmt->setInt(1, uid);
        stmt->setInt(2, uid);
        stmt->setInt(3, uid);
        stmt->setString(4, sinceTime);
        stmt->setInt(5, 20);
        const std::shared_ptr<sql::ResultSet> res(stmt->executeQuery());
        
        while (res->next()) {
            result.push_back(FriendApply::fromFriendApplySearch(res));
        }
        return result;
    } catch (sql::SQLException& e) {
        std::cout << "get user SQLException: " << e.what() << std::endl;
        return {};
    }
}


bool FriendInfoDao::checkFriendApplyExist(const int uid, const int friendId) const {
    if (uid < 0 || friendId < 0) {
        return false;
    }

    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });

    try {
        // 已经存在等待审批的好友申请，不需要重复申请
        // 其他状态用户可以重新发起申请
        const std::string sql = "SELECT 1 FROM friend_apply "
                                "WHERE (friend_apply.uid = ? OR friend_apply.friend_id = ?) "
                                "AND friend_apply.status = 0 ";
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(sql));
        stmt->setInt(1, uid);
        stmt->setInt(2, friendId);
        if (const std::shared_ptr<sql::ResultSet> res(stmt->executeQuery()); res->next()) {
            
            return true;
        }
        
        return false;
    } catch (sql::SQLException& e) {
        std::cout << "get user SQLException: " << e.what() << std::endl;
        return false;
    }
}

bool FriendInfoDao::updateFriendApply(const int uid, const int friendId, const int status, const std::string &msg) {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });

    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
            "INSERT INTO friend_apply (uid, friend_id, msg, status, expire_time) values (?,?,?,?,NOW() + INTERVAL ? DAY) "
            "ON DUPLICATE KEY UPDATE uid = uid, friend_id = friend_id, status = ?, msg = ?, "
            "expire_time = NOW() + INTERVAL ? DAY, create_time = NOW()"));
        stmt->setInt(1, uid);
        stmt->setInt(2, friendId);
        stmt->setString(3, msg);
        stmt->setInt(4, status);
        stmt->setInt(5, 7); // 设置申请过期时间
        stmt->setInt(6, status);
        stmt->setString(7, msg);
        stmt->setInt(8, 7); // 重设申请过期时间
        if (const int rowAffected = stmt->executeUpdate(); rowAffected < 0) {
            
            return false;
        }
        
        return true;
    } catch (sql::SQLException& e) {
        std::cout << "get user SQLException: " << e.what() << std::endl;
        return false;
    }
}

bool FriendInfoDao::getFriendApplyCount(const int uid, int& count) {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });
    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
        "SELECT COUNT(*) AS unread_count "
            "FROM friend_apply "
            "WHERE friend_id = ? AND status = 0 AND expire_time > NOW()"));
        stmt->setInt(1, uid);
        if (const std::unique_ptr<sql::ResultSet> res(stmt->executeQuery()); res->next()) {
            
            count = res->getInt("unread_count");
            return true;
        }
        
        return false;
    } catch (sql::SQLException& e) {
        std::cout << "get user SQLException: " << e.what() << std::endl;
        return false;
    }
}

bool FriendInfoDao::createFriendRelation(const FriendApply &applyInfo) {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    const auto oldCommit = conn->conn_->getAutoCommit();
    Defer defer([this, oldCommit, &conn]() {
        conn->conn_->setAutoCommit(oldCommit);
        pool_->returnConnect(std::move(conn));
    });
    try {
        conn->conn_->setAutoCommit(false);

        const std::unique_ptr<sql::PreparedStatement> stmt_apply(conn->conn_->prepareStatement(
            "UPDATE friend_apply SET status = 1 WHERE uid = ? AND friend_id = ? AND status = 0"));
        stmt_apply->setInt(1, applyInfo.uid);
        stmt_apply->setInt(2, applyInfo.friendId);
        if (const int rowAffected = stmt_apply->executeUpdate(); rowAffected < 0) {
            stmt_apply->close();
            return false;
        }
        stmt_apply->close();

        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
            "INSERT INTO friend_relation (uid, friend_id) values (?,?)"));
        stmt->setInt(1, applyInfo.uid);
        stmt->setInt(2, applyInfo.friendId);
        if (const int rowAffected = stmt->executeUpdate(); rowAffected < 0) {
            
            return false;
        }

        stmt->setInt(1, applyInfo.friendId);
        stmt->setInt(2, applyInfo.uid);
        if (const int rowAffected = stmt->executeUpdate(); rowAffected < 0) {
            
            return false;
        }
        
        conn->conn_->commit();
        return true;
    } catch (sql::SQLException &e) {
        std::cout << "add friend relation SQLException: " << e.what() << std::endl;
        conn->conn_->rollback();
        return false;
    }
}

bool FriendInfoDao::updateFriendRelation(const int uid, const FriendInfo &friendInfo) {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    const auto oldCommit = conn->conn_->getAutoCommit();
    Defer defer([this, oldCommit, &conn]() {
        conn->conn_->setAutoCommit(oldCommit);
        pool_->returnConnect(std::move(conn));
    });

    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
            "UPDATE friend_relation SET " + friendInfo.getUpdateProperty() + " = ? "
            "WHERE uid = ? AND friend_id = ?"));
        if (std::string strValue; friendInfo.getUpdatePropertyStringValue(strValue)) {
            stmt->setString(1, strValue);
        }
        else if (int value; friendInfo.getUpdatePropertyIntValue(value)) {
            stmt->setInt(1, value);
        }
        stmt->setInt(2, uid);
        stmt->setInt(3, friendInfo.friendId);
        if (const int rowAffected = stmt->executeUpdate(); rowAffected < 0) {
            
            return false;
        }
        
        return true;
    } catch (sql::SQLException& e) {
        std::cout << "get user SQLException: " << e.what() << std::endl;
        return false;
    }
}


