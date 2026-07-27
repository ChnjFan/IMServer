//
// Created by Fan on 2026/6/28.
//

#include "ConversationDao.h"
#include "ConfigMgr.h"

constexpr size_t BATCH_CHUNK_SIZE = 50;
constexpr int MAX_DEADLOCK_RETRIES = 3;  // MySQL placeholder 限制，每批最多 50 条
#include "core/ChatMsgNode.h"

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
            stmtResult->close();
            return true;
        }
        stmtResult->close();
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
                                                            const int limit) const {
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

bool ConversationDao::updateConvMessagesStatus(const MessageStatusInfo &info) const {
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
            
            return false;
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

bool ConversationDao::batchCreateMessages(const std::vector<std::shared_ptr<ChatMsgNode>>& nodes,
                                          std::unordered_map<int64_t, int>& id_mapping) {
    auto conn = pool_->getConnect();
    if (!conn) return false;

    const auto oldCommit = conn->conn_->getAutoCommit();
    Defer defer([this, oldCommit, &conn]() {
        conn->conn_->setAutoCommit(oldCommit);
        pool_->returnConnect(std::move(conn));
    });

    // 按 (conv_id, msg_id) 排序，保证所有事务加锁顺序一致，减少死锁
    // nodes 是 const&，需要拷一份 mutable 指针来排序
    std::vector<std::shared_ptr<ChatMsgNode>> sorted_nodes(nodes);
    std::sort(sorted_nodes.begin(), sorted_nodes.end(),
              [](const auto& a, const auto& b) {
                  if (a->msg.convId.value_or("") != b->msg.convId.value_or(""))
                      return a->msg.convId.value_or("") < b->msg.convId.value_or("");
                  return a->msg.msgId < b->msg.msgId;
              });

    // 死锁重试 (使用排序后的副本)
    auto& txn_nodes = sorted_nodes;
    for (int retry = 0; retry < MAX_DEADLOCK_RETRIES; retry++) {
        bool deadlock = false;
        conn->conn_->setAutoCommit(false);

        try {
        // ── Step 1: 批量 INSERT message (分块) ──────────
        for (size_t offset = 0; offset < txn_nodes.size(); offset += BATCH_CHUNK_SIZE) {
            size_t end = std::min(offset + BATCH_CHUNK_SIZE, txn_nodes.size());
            size_t chunk_len = end - offset;

            std::string sql = "INSERT INTO message (conv_id, sender_uid, msg_type, content, msg_id, status) VALUES ";
            for (size_t i = 0; i < chunk_len; i++) {
                if (i > 0) sql += ",";
                sql += "(?,?,?,?,?,?)";
            }
            sql += " ON DUPLICATE KEY UPDATE msg_id=msg_id";

            const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(sql));
            int param = 1;
            for (size_t i = offset; i < end; i++) {
                stmt->setString(param++, txn_nodes[i]->msg.convId.value_or(""));
                stmt->setInt(param++, txn_nodes[i]->msg.fromUid);
                stmt->setInt(param++, txn_nodes[i]->msg.type);
                stmt->setString(param++, txn_nodes[i]->msg.content.value_or(""));
                stmt->setInt(param++, txn_nodes[i]->msg.msgId);
                stmt->setInt(param++, txn_nodes[i]->msg.status);
            }
            stmt->executeUpdate();
            stmt->close();
        }

        // 按 conv_id 分组
        std::unordered_map<std::string, std::vector<std::shared_ptr<ChatMsgNode>>> by_conv;
        for (const auto& n : txn_nodes) {
            by_conv[n->msg.convId.value_or("")].push_back(n);
        }

        // ── Step 2: SELECT 回查 serverId ────────────────
        for (auto& [conv_id, conv_nodes] : by_conv) {
            std::string sql = "SELECT id, msg_id FROM message WHERE conv_id = ? AND msg_id IN (";
            for (size_t i = 0; i < conv_nodes.size(); i++) {
                if (i > 0) sql += ",";
                sql += "?";
            }
            sql += ")";

            const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(sql));
            stmt->setString(1, conv_id);
            for (size_t i = 0; i < conv_nodes.size(); i++) {
                stmt->setInt(i + 2, conv_nodes[i]->msg.msgId);
            }
            const std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
            while (res->next()) {
                int id = res->getInt("id");
                int mid = res->getInt("msg_id");
                int64_t key = static_cast<int64_t>(std::hash<std::string>{}(conv_id)) * 1000000000LL + mid;
                id_mapping[key] = id;
            }
            stmt->close();
        }

        // ── Step 3: 批量 UPDATE conversation ────────────
        {
            // SQL 在所有迭代中相同，预处理一次后复用
            const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
                "UPDATE conversation SET "
                "  last_msg_id = GREATEST(last_msg_id, ?), "
                "  last_msg_content = CASE WHEN ? >= last_msg_id THEN ? ELSE last_msg_content END, "
                "  last_time = NOW() "
                "WHERE conv_id = ?")
            );
            for (auto& [conv_id, conv_nodes] : by_conv) {
                // 找 max_msg_id 对应的节点
                int max_id = 0;
                std::shared_ptr<ChatMsgNode> max_node = nullptr;
                for (const auto& n : conv_nodes) {
                    if (n->msg.msgId > max_id) {
                        max_id = n->msg.msgId;
                        max_node = n;
                    }
                }

                std::string summary;
                if (max_node) {
                    switch (max_node->msg.type) {
                        case 2: summary = "[图片]"; break;
                        case 3: summary = "[文件]"; break;
                        case 4: summary = "[视频]"; break;
                        default: summary = max_node->msg.content.value_or("").substr(0, 100); break;
                    }
                }

                stmt->setInt(1, max_id);
                stmt->setInt(2, max_id);
                stmt->setString(3, summary);
                stmt->setString(4, conv_id);
                stmt->executeUpdate();
            }
            stmt->close();
        }

        // ── Step 4: 批量 UPDATE user_conversation ───────
        {
            // 按 (conv_id, uid) 聚合
            struct uc_key {
                std::string conv_id;
                int uid;
                bool operator==(const uc_key& o) const { return uid == o.uid && conv_id == o.conv_id; }
            };
            struct uc_hash {
                size_t operator()(const uc_key& k) const {
                    return std::hash<std::string>{}(k.conv_id) * 31 + static_cast<size_t>(k.uid);
                }
            };
            struct uc_val {
                int unread_inc = 0;
                bool is_sender = false;
            };
            std::unordered_map<uc_key, uc_val, uc_hash> uc_map;

            for (const auto& n : txn_nodes) {
                std::string cid = n->msg.convId.value_or("");
                uc_key sk{cid, n->msg.fromUid};
                uc_map[sk].is_sender = true;
                uc_key rk{cid, n->msg.toUid};
                uc_map[rk].unread_inc += 1;
            }

            const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
                "UPDATE user_conversation SET "
                "  unread_count = unread_count + ?, "
                "  update_time = NOW() "
                "WHERE conv_id = ? AND uid = ?")
            );
            for (auto& [key, val] : uc_map) {
                stmt->setInt(1, val.is_sender ? 0 : val.unread_inc);
                stmt->setString(2, key.conv_id);
                stmt->setInt(3, key.uid);
                stmt->executeUpdate();
            }
            stmt->close();
        }

        conn->conn_->commit();
        return true;

        } catch (sql::SQLException& e) {
            conn->conn_->rollback();
            if (e.getErrorCode() == 1213 && retry < MAX_DEADLOCK_RETRIES - 1) {
                // MySQL deadlock error code = 1213
                std::cout << "[batchCreateMessages] deadlock, retry " << retry + 1 << std::endl;
                deadlock = true;
                std::this_thread::sleep_for(std::chrono::milliseconds(10 * (retry + 1)));
                continue;
            }
            std::cout << "[batchCreateMessages] SQL exception: " << e.what() << std::endl;
            return false;
        }
        if (!deadlock) break;
    }
    return false;  // 所有重试耗尽
}
