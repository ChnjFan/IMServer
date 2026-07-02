//
// Created by Fan on 2026/6/29.
//

#ifndef IMSERVER_MESSAGEINFO_H
#define IMSERVER_MESSAGEINFO_H

#include <string>
#include <unordered_set>
#include <json/json.h>
#include <jdbc/cppconn/resultset.h>

#include "common/utils/ConversationConvert.h"

enum class MessageType : uint8_t {
    TEXT = 1,       // 文本消息
};

enum class MessageStatus : uint8_t {
    SENDING = 0,       // 发送中
    IS_SEND = 1,       // 已发送
    IS_READ = 2,       // 已读
};

struct MessageInfo {
    int servId = -1;        // 服务端消息 ID
    int fromUid = -1;
    int toUid = -1;
    int msgId = -1;
    int8_t type = -1;
    int8_t status = -1;
    int8_t pad[2] = {0};
    std::optional<std::string> convId;
    std::optional<std::string> content;
    std::optional<std::string> createTime;

    inline static std::unordered_set<std::string> keys{"from_uid", "to_uid", "conv_id", "content",
    "content_type", "msg_id"};

    void fromJson(Json::Value& value);
    void toJson(Json::Value& value) const;

    static MessageInfo fromMessageListSearch(const std::shared_ptr<sql::ResultSet>& result);
};

inline void MessageInfo::fromJson(Json::Value &value) {
    if (value.isMember("from_uid") && !value["from_uid"].isNull()) {
        fromUid = std::stoi(value["from_uid"].asString());
    }
    if (value.isMember("to_uid") && !value["to_uid"].isNull()) {
        toUid = std::stoi(value["to_uid"].asString());
    }
    if (value.isMember("msg_id") && !value["msg_id"].isNull()) {
        msgId = value["msg_id"].asInt();
    }
    if (value.isMember("conv_id") && !value["conv_id"].isNull()) {
        convId = value["conv_id"].asString();
    }
    if (value.isMember("content") && !value["content"].isNull()) {
        content = value["content"].asString();
    }
    if (value.isMember("content_type") && !value["content_type"].isNull()) {
        type = static_cast<int8_t>(value["content_type"].asUInt());
    }
    if (value.isMember("status") && !value["status"].isNull()) {
        status = static_cast<int8_t>(value["status"].asUInt());
    }
    if (value.isMember("create_time") && !value["create_time"].isNull()) {
        createTime = value["create_time"].asString();
    }
}

inline void MessageInfo::toJson(Json::Value &value) const {
    if (servId >= 0) {
        value["server_id"] = servId;
    }
    if (fromUid >= 0) {
        value["from_uid"] = std::to_string(fromUid);
    }
    if (toUid >= 0) {
        value["to_uid"] = std::to_string(toUid);
    }
    if (msgId >= 0) {
        value["msg_id"] = msgId;
    }
    if (type >= 0) {
        value["content_type"] = type;
    }
    if (status >= 0) {
        value["status"] = status;
    }
    if (convId.has_value()) {
        value["conv_id"] = convId.value();
    }
    if (content.has_value()) {
        value["content"] = content.value();
    }
    if (createTime.has_value()) {
        value["create_time"] = createTime.value();
    }
}

inline MessageInfo MessageInfo::fromMessageListSearch(const std::shared_ptr<sql::ResultSet> &result) {
    MessageInfo info;
    info.msgId = result->getInt("id");
    info.convId = result->getString("conv_id");
    info.fromUid = result->getInt("sender_uid");
    info.toUid = getOtherUid(info.convId.value(), info.fromUid);
    info.type = static_cast<int8_t>(result->getInt("msg_type"));
    info.status = static_cast<int8_t>(result->getInt("status"));
    info.content = result->getString("content");
    info.createTime = result->getString("create_time");
    return info;
}

struct MessageStatusInfo {
    int uid = -1;
    int count = -1;
    int lastMsgId = -1;
    int8_t status = -1;
    int8_t pad[3] = {0};
    std::optional<std::string> convId;

    void fromJson(Json::Value &value);
};

inline void MessageStatusInfo::fromJson(Json::Value &value) {
    if (value.isMember("count") && !value["count"].isNull()) {
        count = value["count"].asInt();
    }
    if (value.isMember("last_msg_id") && !value["last_msg_id"].isNull()) {
        lastMsgId= value["last_msg_id"].asInt();
    }
    if (value.isMember("status") && !value["status"].isNull()) {
        status = static_cast<int8_t>(value["status"].asInt());
    }
    if (value.isMember("conv_id") && !value["conv_id"].isNull()) {
        convId = value["conv_id"].asString();
    }
    if (value.isMember("uid") && !value["uid"].isNull()) {
        uid = std::stoi(value["uid"].asString());
    }
}

#endif //IMSERVER_MESSAGEINFO_H
