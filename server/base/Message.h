//
// Created by fan on 24-11-17.
//

#ifndef IMSERVER_MESSAGE_H
#define IMSERVER_MESSAGE_H

#include <memory>
#include <string>
#include "ByteStream.h"
#include "google/protobuf/message.h"

namespace Base {

    class Message;
    typedef std::shared_ptr<Message> MessagePtr;
    typedef std::shared_ptr<google::protobuf::Message> ProtobufMsgPtr;

/**
 * @class Message
 * @brief 消息基础类
 * @note 定义消息基础类，用于所有消息类型的基类
 */
class Message {
public:
    Message();

    static MessagePtr getMessage(Base::ByteStream &data);

    ProtobufMsgPtr deserialize();
    uint32_t serialize(char *buf, uint32_t bufSize);

    uint32_t size();

private:

private:
    static constexpr uint32_t DEFAULT_BODY_LEN = 1024;
    static constexpr uint32_t DEFAULT_HEADER_LEN = 3*sizeof(uint32_t);

    /**
     * @brief 消息长度
     */
    uint32_t length;
    /**
     * @brief 消息类型长度
     */
    uint32_t typeLen;
    /**
     * @brief 消息类型名
     */
    std::string typeName;
    /**
     * @brief 消息校验和
     */
    uint32_t checkSum;
    Base::ByteStream body;
};

}
#endif //IMSERVER_MESSAGE_H
