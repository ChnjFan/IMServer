//
// Created by Fan on 2026/6/28.
//

#ifndef IMSERVER_CONVERSATIONDAO_H
#define IMSERVER_CONVERSATIONDAO_H

#include "MysqlPool.h"
#include "common/model/ConversationInfo.h"
#include "common/model/MessageInfo.h"

class ConversationDao {
public:
    ConversationDao();
    ~ConversationDao();

    bool createConversation(const ConversationInfo& info, std::string& result) const;

    bool createMessage(const MessageInfo & info, int& result) const;

    [[nodiscard]] bool updateMessageStatus(int id, MessageStatus status) const;

    [[nodiscard]] std::vector<ConversationInfo> selectConversationList(int uid, const std::string & sinceTime) const;

    std::vector<MessageInfo> selectMessageList(const std::string & convId, int since_msg_id, int limit);

    bool updateConvMessagesStatus(const MessageStatusInfo & info);

private:
    std::unique_ptr<MysqlPool> pool_;
};


#endif //IMSERVER_CONVERSATIONDAO_H