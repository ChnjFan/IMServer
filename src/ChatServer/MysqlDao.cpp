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

bool MysqlDao::getFriendList(int uid, FriendInfoList &friendList, const int start, const int size) {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });

    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
            "SELECT relation.friend_id, relation.alias, relation.status, relation.is_star, relation.is_hide, user.name, user.email "
                    "FROM friend_relation as relation join user on relation.friend_id = user.uid WHERE relation.uid = ? "
                    "AND relation.id > ? ORDER by relation.id ASC LIMIT ? "));
        stmt->setInt(1, uid);
        stmt->setInt(2, start);
        stmt->setInt(3, size);
        const std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
        while (res->next()) {
            std::cout << "[getFriendList] Get user: " << uid << std::endl;
            auto user = std::make_shared<FriendInfo>();
            user->uid = res->getInt("friend_id");
            user->isStar = res->getInt("is_star");
            user->isHidden = res->getInt("is_hide");
            user->status = res->getInt("status");
            user->email = res->getString("email");
            user->name = res->getString("name");
            user->alias = res->getString("alias");
            friendList.push_back(std::move(user));
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
