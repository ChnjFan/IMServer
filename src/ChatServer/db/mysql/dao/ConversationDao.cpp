//
// Created by Fan on 2026/6/28.
//

#include "ConversationDao.h"
#include "ConfigMgr.h"

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

bool ConversationDao::createConversation(const ConversationInfo &info, std::string &result) {
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
            std::cout << "Result: " << result << std::endl;
            return true;
        }
        return false;
    } catch (sql::SQLException &e) {
        std::cout << "register user SQLException: " << e.what() << std::endl;
        return false;
    }
}
