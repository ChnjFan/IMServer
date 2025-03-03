//
// Created by fan on 25-2-26.
//

#ifndef IMSERVER_MESSAGE_H
#define IMSERVER_MESSAGE_H

#include <utility>
#include "google/protobuf/message.h"
#include "Poco/Checksum.h"
#include "Buffer.h"

#include "IM.ServerMsg.pb.h"

namespace Base {

class Message;
using MessagePtr = std::shared_ptr<Message>;

/**
 * Message 消息格式：
 * |+++++++++++++|+++++++++++|++++++++++|++++++++++++++|
 * | MessageSize | ProtoName | Protobuf | CRC CheckSum |
 * |+++++++++++++|+++++++++++|++++++++++|++++++++++++++|
 **/
class Message {
public:
    explicit Message(int32_t size, int32_t checkSum, google::protobuf::Message *pMsg) : size_(size), checkSum_(checkSum) {
        message_.reset(pMsg);
    }

    static MessagePtr parseMessage(Buffer &buffer) {
        if (!checkMessage(buffer)) {
            return nullptr;
        }

        int32_t size = buffer.peekInt32();
        const char* data = buffer.peek() + MESSAGE_HEADER_SIZE;
        google::protobuf::Message *pMsg = parse(data, size);
        if (nullptr == pMsg) {
            throw Base::Exception("Error message.");
        }

        buffer.retrieve(size + MESSAGE_HEADER_SIZE);
        Poco::Checksum crc32(Poco::Checksum::TYPE_CRC32);
        crc32.update(data, size);
        int32_t expectCheckSum = buffer.peekInt32();
        if (crc32.checksum() != (uint32_t)expectCheckSum) {
            throw Base::Exception("Error message, checksum is: "
                    + std::to_string(buffer.peekInt32()));
        }
        buffer.retrieveInt32();
        MessagePtr result = std::make_shared<Message>(size, expectCheckSum, pMsg);
        return result;
    }

    static Buffer serializeMessage(MessagePtr message) {
        Buffer buffer;
        if (message == nullptr)
            return buffer;
        buffer.appendInt32(message->getSize());
        std::string type = message->getMessage()->GetTypeName();
        buffer.append(type.c_str(), type.length() + 1);
        message->getMessage()->SerializeToArray(buffer.beginWrite(), (int)buffer.writableBytes());
        buffer.appendInt32(message->getCheckSum());

        return buffer;
    }

    int32_t getSize() const {
        return size_;
    }

    void setSize(int32_t size) {
        size_ = size;
    }

    int32_t getCheckSum() const {
        return checkSum_;
    }

    void setCheckSum(int32_t checkSum) {
        checkSum_ = checkSum;
    }

    const std::shared_ptr<google::protobuf::Message> &getMessage() const {
        return message_;
    }

    void setMessage(const std::shared_ptr<google::protobuf::Message> &message) {
        message_ = message;
    }

private:
    /*解析protobuf消息*/
    static google::protobuf::Message* parse(const char *data, int size) {
        const char* type = data;
        int typeSize = (int)strlen(type) + 1;

        if (typeSize >= size) {
            throw Base::Exception("Error message: typeName size: " + std::to_string(typeSize));
        }
        IM::ServerMsg::LoginRequest login;

        google::protobuf::Message *pMsg = createMessage(type);
        if (pMsg) {
            pMsg->ParseFromArray(data + typeSize, size - typeSize);
        }
        return pMsg;
    }

    /*根据消息类型创建protobuf消息*/
    static google::protobuf::Message* createMessage(const char* typeName)
    {
        google::protobuf::Message* message = nullptr;
        const google::protobuf::Descriptor* descriptor =
                google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(typeName);
        if (descriptor)
        {
            const google::protobuf::Message* prototype =
                    google::protobuf::MessageFactory::generated_factory()->GetPrototype(descriptor);
            if (prototype)
            {
                message = prototype->New();
            }
        }
        return message;
    }

    static bool checkMessage(Buffer &buffer) {
        size_t size = buffer.peekInt32();
        if (size + MESSAGE_HEADER_SIZE + MESSAGE_CHECKSUM_SIZE > buffer.readableBytes()) {
            return false;
        }

        return true;
    }

    static constexpr size_t MESSAGE_HEADER_SIZE = sizeof(int32_t);
    static constexpr size_t MESSAGE_CHECKSUM_SIZE = sizeof(int32_t);
    int32_t size_;
    int32_t checkSum_;
    std::shared_ptr<google::protobuf::Message> message_;
};

}

#endif //IMSERVER_MESSAGE_H
