//
// Created by Fan on 2026/6/28.
//

#ifndef IMSERVER_CONVERSATIONDAO_H
#define IMSERVER_CONVERSATIONDAO_H

#include "MysqlPool.h"
#include "common/model/ConversationInfo.h"

class ConversationDao {
public:
    ConversationDao();
    ~ConversationDao();

    bool createConversation(const ConversationInfo& info, std::string& result);

private:
    std::unique_ptr<MysqlPool> pool_;
};


#endif //IMSERVER_CONVERSATIONDAO_H