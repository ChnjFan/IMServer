//
// Created by Fan on 2026/6/28.
//

#ifndef IMSERVER_CONVERSATIONDAO_H
#define IMSERVER_CONVERSATIONDAO_H

#include "MysqlPool.h"
#include "common/model/ConversationInfo.h"
#include "common/model/MessageInfo.h"
#include "core/ChatMsgNode.h"

class ConversationDao {
public:
    ConversationDao();
    ~ConversationDao();

    bool createConversation(const ConversationInfo& info, std::string& result) const;

    bool createMessage(const MessageInfo & info, int& result) const;

    [[nodiscard]] bool updateMessageStatus(int id, MessageStatus status) const;

    [[nodiscard]] std::vector<ConversationInfo> selectConversationList(int uid, const std::string & sinceTime) const;

    std::vector<MessageInfo> selectMessageList(const std::string & convId, int since_msg_id, int limit) const;

    bool updateConvMessagesStatus(const MessageStatusInfo & info) const;


    // ── 批量异步入库 ─────────────────────────────────────
    /**
     * @brief 批量插入聊天消息 + 更新会话元数据 + 更新用户会话未读计数。
     */
    bool batchCreateMessages(const std::vector<std::shared_ptr<ChatMsgNode>>& nodes,
                             std::unordered_map<int64_t, int>& id_mapping);

private:
    std::unique_ptr<MysqlPool> pool_;
};


#endif //IMSERVER_CONVERSATIONDAO_H
