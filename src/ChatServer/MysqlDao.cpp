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

bool MysqlDao::updateUserInfo(const UserInfo &user_info) {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });
    std::vector<std::string> setParts;
    std::vector<std::pair<int, std::string>> strParams;

    if (!user_info.name.empty()) {
        setParts.emplace_back("name = ?");
        strParams.emplace_back(strParams.size() + 1, user_info.name);
    }

    if (!user_info.email.empty()) {
        setParts.emplace_back("email = ?");
        strParams.emplace_back(strParams.size() + 1, user_info.email);
    }

    if (!user_info.avatarUrl.empty()) {
        setParts.emplace_back("avatar = ?");
        strParams.emplace_back(strParams.size() + 1, user_info.avatarUrl);
    }

    if (setParts.empty()) {
        return false;
    }

    std::string sql = "UPDATE user SET ";
    for (size_t i = 0; i < setParts.size(); i++) {
        if (i > 0) sql += ",";
        sql += setParts[i];
    }
    sql += ", update_time = NOW() WHERE uid = ?";

    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(sql));
        for (auto& [index, value] : strParams) {
            stmt->setString(index, value);
        }
        stmt->setInt(strParams.size() + 1, user_info.uid);
        if (const int rowAffected = stmt->executeUpdate(); rowAffected < 0) {
            return false;
        }
        return true;
    } catch (sql::SQLException& e) {
        std::cout << "update user info SQLException: " << e.what() << std::endl;
        return false;
    }
}

bool MysqlDao::getUserInfo(UserInfo &user_info) {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });
    try {
        const std::string part = getSearchPart(user_info);
        if (part.empty()) {
            return false;
        }
        const std::string sql = "SELECT * FROM user WHERE " + part;
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(sql));
        if (user_info.uid >= 0) {
            stmt->setInt(1, user_info.uid);
        }
        else if (!user_info.email.empty()) {
            stmt->setString(1, user_info.email);
        }
        else if (!user_info.name.empty()) {
            stmt->setString(1, user_info.name);
        }

        const std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        while (res->next()) {
            user_info.uid = res->getInt("uid");
            user_info.gender = res->getInt("gender");
            user_info.name = res->getString("name");
            user_info.email = res->getString("email");
            user_info.avatarUrl = res->getString("avatar");
            user_info.createTime = res->getString("create_time");
            break;
        }
        return true;
    } catch (sql::SQLException& e) {
        std::cout << "get user info SQLException: " << e.what() << std::endl;
        return false;
    }
}

bool MysqlDao::getUserProfileInfo(const int uid, UserProfileInfo &user_profile_info) {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });
    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
            "SELECT * FROM user_profile WHERE uid = ?"));
        stmt->setInt(1, uid);

        const std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        while (res->next()) {
            user_profile_info.signature = res->getString("signature");
            user_profile_info.birthday = res->getString("birthday");
            user_profile_info.region = res->getString("region");
            user_profile_info.selfIntro = res->getString("self_intro");
            user_profile_info.privacyFriend = res->getInt("privacy_friend");
            user_profile_info.privacyChat = res->getInt("privacy_chat");
            user_profile_info.privacyBacklist = res->getInt("blacklist_switch");
            user_profile_info.ex = res->getString("extra_json");
            user_profile_info.createTime = res->getString("create_time");
            user_profile_info.updateTime = res->getString("update_time");
            break;
        }
        return true;
    } catch (sql::SQLException& e) {
        std::cout << "get user: " << uid << " full info SQLException: " << e.what() << std::endl;
        return false;
    }
}

bool MysqlDao::getFriendRelation(const int uid, const int friendId, FriendRelation &fr) const {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });
    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
            "SELECT * FROM friend_relation WHERE uid = ? AND friend_id = ?"));
        stmt->setInt(1, uid);
        stmt->setInt(2, friendId);

        const std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        while (res->next()) {
            if (res->getInt("uid") != uid || res->getInt("friend_id") != friendId) {
                continue;
            }
            fr.alias = res->getString("alias");
            fr.status = res->getInt("status");
            fr.isStar = res->getInt("is_star");
            fr.isHide = res->getInt("is_hide");
            fr.createTime = res->getString("created_time");
            fr.updateTime = res->getString("update_time");
            return true;
        }
        return false;
    } catch (sql::SQLException& e) {
        std::cout << "get user: " << uid << " friend " << friendId << " info SQLException: " << e.what() << std::endl;
        return false;
    }
}

bool MysqlDao::checkFriendRelation(const int uid, const int friendId) const {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });
    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
            "SELECT 1 FROM friend_relation WHERE uid = ? AND friend_id = ? LIMIT 1"));
        stmt->setInt(1, uid);
        stmt->setInt(2, friendId);

        const std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        while (res->next()) {
            if (res->getInt("uid") != uid || res->getInt("friend_id") != friendId) {
                continue;
            }
            return true;
        }
        return false;
    } catch (sql::SQLException& e) {
        std::cout << "get user: " << uid << " friend " << friendId << " info SQLException: " << e.what() << std::endl;
        return false;
    }
}

bool MysqlDao::checkFriendApply(const int uid, const int friendId) const {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });

    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
            "SELECT 1 FROM friend_apply WHERE uid = ? AND friend_id = ? AND status != 3 LIMIT 1"));
        stmt->setInt(1, uid);
        stmt->setInt(2, friendId);

        const std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        while (res->next()) {
            if (res->getInt("uid") != uid || res->getInt("friend_id") != friendId) {
                continue;
            }
            return true;
        }
        return false;
    } catch (sql::SQLException& e) {
        std::cout << "get user SQLException: " << e.what() << std::endl;
        return false;
    }
}

bool MysqlDao::updateFriendApply(const int uid, const int friendId, const int status, const std::string& msg) const {
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
            "ON DUPLICATE KEY UPDATE uid = uid, friend_id = friend_id, status = ?"));
        stmt->setInt(1, uid);
        stmt->setInt(2, friendId);
        stmt->setString(3, msg);
        stmt->setInt(4, status);
        stmt->setInt(5, 7); // todo 这里改为可配置
        stmt->setInt(6, status);
        if (const int rowAffected = stmt->executeUpdate(); rowAffected < 0) {
            return false;
        }
        return true;
    } catch (sql::SQLException& e) {
        std::cout << "get user SQLException: " << e.what() << std::endl;
        return false;
    }
}

bool MysqlDao::getFriendReplyCount(const int uid, int &count) {
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
        const std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        while (res->next()) {
            count = res->getInt("unread_count");
            return true;
        }
        return false;
    } catch (sql::SQLException& e) {
        std::cout << "get user SQLException: " << e.what() << std::endl;
        return false;
    }
}

bool MysqlDao::getFriendApplyList(const int uid, const int since_id, std::vector<FriendApplyInfo> &applyList) {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });

    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
            "SELECT apply.id as apply_id, "
            "apply.uid as apply_uid, "
            "apply.friend_id as apply_friend_id, "
            "apply.msg as apply_msg, "
            "apply.status as apply_status, "
            "apply.expire_time as apply_expire_time, "
            "apply.create_time as apply_create_time, "
            "apply.update_time as apply_update_time, "
            "user.uid as user_uid, "
            "user.name as name, "
            "user.email as email, "
            "user.avatar as avatar, "
            "user.gender as gender "
            "FROM friend_apply as apply JOIN user "
            "ON user.uid = IF(apply.uid = ?, apply.friend_id, apply.uid)"
            "WHERE (apply.uid = ? OR apply.friend_id = ?) "
            "AND apply.id > ? ORDER by apply.id ASC LIMIT ? "));
        stmt->setInt(1, uid);
        stmt->setInt(2, uid);
        stmt->setInt(3, uid);
        stmt->setInt(4, since_id);
        stmt->setInt(5, 20);
        const std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        while (res->next()) {
            FriendApplyInfo applyInfo;
            applyInfo.id = res->getInt("apply_id");
            applyInfo.uid = res->getInt("apply_uid");
            applyInfo.friendId = res->getInt("apply_friend_id");
            applyInfo.msg = res->getString("apply_msg");
            applyInfo.status = res->getInt("apply_status");
            applyInfo.expiresTime = res->getString("apply_expire_time");
            applyInfo.createdTime = res->getString("apply_create_time");
            applyInfo.updateTime = res->getString("apply_update_time");
            applyInfo.userInfo.uid = res->getInt("user_uid");
            applyInfo.userInfo.name = res->getString("name");
            applyInfo.userInfo.email = res->getString("email");
            applyInfo.userInfo.avatarUrl = res->getString("avatar");
            applyInfo.userInfo.gender = res->getInt("gender");
            applyList.push_back(applyInfo);
        }
        return !applyList.empty();
    } catch (sql::SQLException& e) {
        std::cout << "get user SQLException: " << e.what() << std::endl;
        return false;
    }
}

bool MysqlDao::updateFriendRelation(const FriendInfo &friendInfo) {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });
    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
            "INSERT INTO friend_relation (uid, friend_id, alias, status, is_star, is_hide) values (?,?,?,?,?,?) "
            "ON DUPLICATE KEY UPDATE uid = uid, friend_id = friend_id, (alias, status, is_star, is_hide) values (?,?,?,?)"));
        stmt->setInt(1, friendInfo.uid);
        stmt->setInt(2, friendInfo.friendId);
        stmt->setString(3, friendInfo.relation.alias);
        stmt->setInt(4, friendInfo.relation.status);
        stmt->setInt(5, friendInfo.relation.isStar);
        stmt->setInt(6, friendInfo.relation.isHide);
        stmt->setString(7, friendInfo.relation.alias);
        stmt->setInt(8, friendInfo.relation.status);
        stmt->setInt(9, friendInfo.relation.isStar);
        stmt->setInt(10, friendInfo.relation.isHide);
        if (const int rowAffected = stmt->executeUpdate(); rowAffected < 0) {
            return false;
        }
        return true;
    } catch (sql::SQLException &e) {
        std::cout << "add friend relation SQLException: " << e.what() << std::endl;
        return false;
    }
}

bool MysqlDao::createFriendRelation(const FriendInfo &friendInfo) {
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
        stmt_apply->setInt(1, friendInfo.friendId);
        stmt_apply->setInt(2, friendInfo.uid);
        if (const int rowAffected = stmt_apply->executeUpdate(); rowAffected < 0) {
            return false;
        }

        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
            "INSERT INTO friend_relation (uid, friend_id, alias) values (?,?,?)"));
        stmt->setInt(1, friendInfo.uid);
        stmt->setInt(2, friendInfo.friendId);
        stmt->setString(3, friendInfo.relation.alias);
        if (const int rowAffected = stmt->executeUpdate(); rowAffected < 0) {
            return false;
        }

        stmt->setInt(1, friendInfo.friendId);
        stmt->setInt(2, friendInfo.uid);
        stmt->setString(3, "");
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

bool MysqlDao::getFriendList(int uid, int sinceId, std::vector<FriendInfo> &friendList) {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });

    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
            "SELECT relation.friend_id AS rel_fid, relation.alias AS rel_alias, relation.status AS rel_status, "
                    "relation.is_star AS rel_star, relation.id AS rel_id, user.name AS user_name, user.avatar AS user_avatar "
                    "FROM friend_relation as relation join user on relation.friend_id = user.uid "
                    "WHERE relation.uid = ? AND relation.is_hide = 0 AND relation.id > ? "
                    "ORDER by relation.id ASC LIMIT ? "));
        stmt->setInt(1, uid);
        stmt->setInt(2, sinceId);
        stmt->setInt(3, 200);
        const std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        while (res->next()) {
            FriendInfo info;
            info.uid = uid;
            info.friendId = res->getInt("rel_fid");
            info.userInfo.baseInfo.uid = info.friendId;
            info.userInfo.baseInfo.name = res->getString("user_name");
            info.userInfo.baseInfo.avatarUrl = res->getString("user_avatar");
            info.relation.id = res->getInt("rel_id");
            info.relation.alias = res->getString("rel_alias");
            info.relation.status = res->getInt("rel_status");
            info.relation.isStar = res->getInt("rel_star");
            friendList.push_back(info);
        }
        return true;
    } catch (sql::SQLException& e) {
        std::cout << "get user SQLException: " << e.what() << std::endl;
        return false;
    }
}

bool MysqlDao::addConversation(int uid, int to, const std::string &convId, int convType) {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });

    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
            "INSERT INTO user_conversation (uid, conv_id, conv_type, to_uid) values (?,?,?,?) "
            "ON DUPLICATE KEY UPDATE uid = uid, conv_id = conv_id"));
        std::cout << "Insert user_conversation (" << uid << ", " << to << ", " << convId << ")" << std::endl;
        stmt->setInt(1, uid);
        stmt->setString(2, convId);
        stmt->setInt(3, convType);
        stmt->setInt(4, to);
        if (const int rowAffected = stmt->executeUpdate(); rowAffected < 0) {
            return false;
        }
        return true;
    } catch (sql::SQLException& e) {
        std::cout << "addConversation SQLException: " << e.what() << std::endl;
        return false;
    }
}

bool MysqlDao::getConversation(int uid, ConversationList &convList) {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });
    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
        "SELECT * FROM user_conversation WHERE uid = ?"));
        stmt->setInt(1, uid);
        const std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        while (res->next()) {
            const auto conv = std::make_shared<ConversationInfo>();
            conv->conv_id = res->getString("conv_id");
            conv->conv_type = res->getInt("conv_type");
            conv->to_uid = res->getInt("to_uid");
            conv->unread_count = res->getInt("unread_count");
            conv->is_top = res->getInt("is_top");
            conv->is_mute = res->getInt("is_mute");
            conv->last_msg_id = res->getInt("last_msg_id");
            conv->last_msg = res->getString("last_msg_content");
            conv->last_time = res->getString("last_time");
            convList.push_back(conv);
        }
        return true;
    } catch (sql::SQLException& e) {
        std::cout << "get user SQLException: " << e.what() << std::endl;
        return false;
    }
}

bool MysqlDao::addHistoryMessage(const MessageInfo &message) {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });
    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
            "CALL save_chat_message(?,?,?,?,?,?,?,@result)"));
        stmt->setString(1, message.conv_id);
        stmt->setInt(2, message.sender_uid);
        stmt->setInt(3, message.receiver_uid);
        stmt->setInt(4, message.msg_type);
        stmt->setInt(5, message.msg_id);
        stmt->setInt(6, message.status);
        stmt->setString(7, message.content);
        stmt->execute();

        const std::unique_ptr<sql::Statement> stmtResult(conn->conn_->createStatement());
        if (const std::unique_ptr<sql::ResultSet> res(stmtResult->executeQuery("SELECT @result AS result"));
                res->next()) {
            const int result = res->getInt("result");
            std::cout << "Result: " << result << std::endl;
            return result > 0;
        }
        return false;
    } catch (sql::SQLException &e) {
        std::cout << "register user SQLException: " << e.what() << std::endl;
        return false;
    }
}

std::string MysqlDao::getSearchPart(const UserInfo &user_info) {
    if (user_info.uid >= 0) {
        return "uid = ?";
    }
    if (!user_info.email.empty()) {
        return "email = ?";
    }
    if (!user_info.name.empty()) {
        return "name = ?";
    }
    return "";
}
