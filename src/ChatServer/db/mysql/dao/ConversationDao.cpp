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
constexpr std::string_view MESSAGE_INFO_PARTS_END = "id, conv_id, sender_uid, msg_type, content, status, create_time ";

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
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });

    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
            "UPDATE message SET status = ? WHERE id = ?"));
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

std::vector<MessageInfo> ConversationDao::selectMessageList(const std::string &convId, const int since_msg_id,
                                                            const int limit) {
    auto conn = pool_->getConnect();
    if (!conn) {
        return {};
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });

    try {
        std::vector<MessageInfo> result;
        const std::string sql = "SELECT " + std::string(MESSAGE_INFO_PARTS_END)
                                + "FROM message "
                                "WHERE conv_id = ? AND id > ? "
                                "ORDER by id ASC LIMIT ? ";
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(sql));
        stmt->setString(1, convId);
        stmt->setInt(2, since_msg_id);
        stmt->setInt(3, limit);
        const std::shared_ptr<sql::ResultSet> res(stmt->executeQuery());
        while (res->next()) {
            result.push_back(MessageInfo::fromMessageListSearch(res));
        }
        return result;
    } catch (sql::SQLException& e) {
        std::cout << "selectConversationList SQLException: " << e.what() << std::endl;
        return {};
    }
}

bool ConversationDao::updateConvMessagesStatus(const MessageStatusInfo &info) {
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

        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
            "UPDATE message SET status = ? "
            "WHERE id <= ? AND conv_id = ? AND sender_uid = ? ORDER BY id LIMIT ?"));
        stmt->setInt(1, info.status);
        stmt->setInt(2, info.lastMsgId);
        stmt->setString(3, info.convId.value());
        // 消息状态应该对方才是发送方，接收方来更新发送方的消息状态
        const auto senderUid = getOtherUid(info.convId.value(), info.uid);
        stmt->setInt(4, senderUid);
        stmt->setInt(5, info.count);
        if (const int rowAffected = stmt->executeUpdate(); rowAffected < 0) {
            throw sql::SQLException("update status failed");
        }

        // 获取未读计数
        const std::unique_ptr<sql::PreparedStatement> stmt_select(conn->conn_->prepareStatement(
            "SELECT unread_count FROM user_conversation "
            "WHERE uid = ? AND conv_id = ?"));
        stmt_select->setInt(1, info.uid);
        stmt_select->setString(2, info.convId.value());
        int unread = -1;
        if (const std::unique_ptr<sql::ResultSet> res(stmt_select->executeQuery()); res->next()) {
            unread = res->getInt("unread_count");
        }

        unread = (unread - info.count > 0) ? unread : 0;
        // 更新自己会话的未读计数
        const std::unique_ptr<sql::PreparedStatement> stmt_update(conn->conn_->prepareStatement(
            "UPDATE user_conversation SET unread_count = ? "
            "WHERE uid = ? AND conv_id = ?"));
        stmt_update->setInt(1, unread);
        stmt_update->setInt(2, info.uid);
        stmt_update->setString(3, info.convId.value());
        if (const int rowAffected = stmt_update->executeUpdate(); rowAffected < 0) {
            throw sql::SQLException("update status failed");
        }

        conn->conn_->commit();
        return true;
    } catch (sql::SQLException& e) {
        std::cout << "updateMessageStatus SQLException: " << e.what() << std::endl;
        conn->conn_->rollback();
        return false;
    }
}
