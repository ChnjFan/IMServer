//
// Created by Fan on 2026/6/28.
//

#include "ConversationDao.h"
#include "ConfigMgr.h"

constexpr std::string_view CONVERSATION_INFO_PARTS = "conversation.conv_id, conversation.conv_type, "
                                                     "conversation.last_msg_id, conversation.last_msg_content, "
                                                     "conversation.last_time, conversation.update_time, "
                                                     "conversation.create_time, "
                                                     "user_conversation.unread_count, user_conversation.is_top, "
                                                     "user_conversation.is_mute ";

ConversationDao::ConversationDao() {
    auto& conf = ConfigMgr::getInstance();
    const auto& host = conf["Mysql"]["Host"];
    const auto& port = conf["Mysql"]["Port"];
    const auto& user = conf["Mysql"]["User"];
    const auto& password = conf["Mysql"]["Password"];
    const auto& schema = conf["Mysql"]["Schema"];
    pool_ = std::make_unique<MysqlPool>(host + ":" + port, user, password, schema);
}

ConversationDao::~ConversationDao() {
    pool_->close();
}

bool ConversationDao::createConversation(const ConversationInfo &info, std::string &result) const {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });
    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
            "CALL create_conversation(?,?,?,@result)"));
        stmt->setString(1, info.convId);
        stmt->setInt(2, info.uid);
        stmt->setInt(3, info.friendId);

        stmt->execute();

        const std::unique_ptr<sql::Statement> stmtResult(conn->conn_->createStatement());
        if (const std::unique_ptr<sql::ResultSet> res(stmtResult->executeQuery("SELECT @result AS result"));
                res->next()) {
            result = res->getString("result");
            return true;
        }
        return false;
    } catch (sql::SQLException &e) {
        std::cout << "register user SQLException: " << e.what() << std::endl;
        return false;
    }
}

bool ConversationDao::createMessage(const MessageInfo &info, int& result) const {
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
        stmt->setString(1, info.convId.value());
        stmt->setInt(2, info.fromUid);
        stmt->setInt(3, info.toUid);
        stmt->setInt(4, info.type);
        stmt->setInt(5, info.msgId);
        stmt->setInt(6, info.status);
        stmt->setString(7, info.content.value());

        stmt->execute();

        const std::unique_ptr<sql::Statement> stmtResult(conn->conn_->createStatement());
        if (const std::unique_ptr<sql::ResultSet> res(stmtResult->executeQuery("SELECT @result AS result"));
                res->next()) {
            result = res->getInt("result");
            std::cout << "Result: " << result << std::endl;
            return true;
        }
        return false;
    } catch (sql::SQLException &e) {
        std::cout << "createMessage SQLException: " << e.what() << std::endl;
        return false;
    }
}

bool ConversationDao::updateMessageStatus(const int id, const MessageStatus status) const {
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
            "UPDATE user_profile SET status = ? WHERE id = ?"));
        stmt->setInt(1, static_cast<int>(status));
        stmt->setInt(2, id);
        if (const int rowAffected = stmt->executeUpdate(); rowAffected < 0) {
            return false;
        }
        return true;
    } catch (sql::SQLException& e) {
        std::cout << "updateMessageStatus SQLException: " << e.what() << std::endl;
        return false;
    }
}

std::vector<ConversationInfo> ConversationDao::selectConversationList(const int uid, const std::string &sinceTime) const {
    auto conn = pool_->getConnect();
    if (!conn) {
        return {};
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });

    try {
        std::vector<ConversationInfo> result;
        const std::string sql = "SELECT " + std::string(CONVERSATION_INFO_PARTS)
                                + "FROM user_conversation "
                                "INNER JOIN conversation "
                                "ON user_conversation.conv_id = conversation.conv_id "
                                "WHERE user_conversation.uid = ? AND user_conversation.update_time > ? "
                                "ORDER by user_conversation.update_time ASC LIMIT ? ";
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(sql));
        stmt->setInt(1, uid);
        stmt->setString(2, sinceTime);
        stmt->setInt(3, 50);
        const std::shared_ptr<sql::ResultSet> res(stmt->executeQuery());
        while (res->next()) {
            result.push_back(ConversationInfo::fromConversationListSearch(res));
        }
        return result;
    } catch (sql::SQLException& e) {
        std::cout << "selectConversationList SQLException: " << e.what() << std::endl;
        return {};
    }
}
