//
// Created by fan on 24-11-17.
//

#ifndef IMSERVER_MESSAGE_H
#define IMSERVER_MESSAGE_H

#include <memory>
#include <string>
#include "google/protobuf/message.h"
#include "Poco/Buffer.h"

namespace Base {

class Message;
using MessagePtr = std::shared_ptr<Message>;
using ProtobufMsgPtr = std::shared_ptr<google::protobuf::Message>;
using ByteBuffer = Poco::Buffer<unsigned char>;

/**
 * @class Message
 * @brief 消息基础类
 * @note 定义消息基础类，用于所有消息类型的基类
 */
class Message {
public:
    Message();
    Message(ByteBuffer& body, std::string typeName);

    /**
     * @brief 消息解码
     * @param data 接收数据
     * @return 请求消息
     * @throws Exception 消息解码异常
     */
    static MessagePtr getMessage(ByteBuffer &data);

    /**
     * @brief 消息反序列化
     * @return protobuf 消息类型
     */
    ProtobufMsgPtr deserialize();

    /**
     * @brief 消息序列化
     * @param buf 序列化消息内存缓冲区
     * @param bufSize 缓冲区长度
     * @return 序列化消息长度
     */
    uint32_t serialize(unsigned char *buf, uint32_t bufSize);

    uint32_t size();

    std::string& getTypeName();

private:
    static constexpr uint32_t MAX_MESSAGE_LEN = 5 * 1024;
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
    /**
     * @brief 消息体
     */
    ByteBuffer body;
};

}
#endif //IMSERVER_MESSAGE_H
