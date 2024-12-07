//
// Created by fan on 24-11-17.
//

#include <memory>
#include <cstring>
#include "Message.h"
#include "Exception.h"
#include "Poco/Checksum.h"

Base::Message::Message(): length(0), typeLen(0), typeName(""), checkSum(0), body(DEFAULT_BODY_LEN) { }

Base::Message::Message(Base::ByteStream &body, std::string &typeName)
                    : length(2*sizeof(uint32_t) + typeName.length() + 1 + body.size())
                    , typeLen(typeName.length() + 1)
                    , typeName(typeName)
                    , checkSum(0)
                    , body(body) {
    Poco::Checksum checker(Poco::Checksum::TYPE_ADLER32);
    checker.update(body.data(), body.size());
    checkSum = checker.checksum();
}

Base::MessagePtr Base::Message::getMessage(Base::ByteStream &data) {
    uint32_t len = data.peekUint32();

    // 数据不完整，继续等待接收数据。其他情况消息内容有问题，需要跳过消息。
    if (len + sizeof(uint32_t) > static_cast<uint32_t>(data.size()))
        return nullptr;

    Base::MessagePtr pMessage = std::make_shared<Message>();

    // 读取消息头
    pMessage->length = data.readUint32();
    pMessage->typeLen = data.readUint32();

    pMessage->typeName = data.readString(pMessage->typeLen);

    Base::ByteStream temp = data.read(pMessage->length - pMessage->typeLen - 2*sizeof(uint32_t));
    pMessage->body = temp;

    pMessage->checkSum = data.readUint32();
    // 计算校验和，校验失败返回空，此时该消息也从缓冲区中读完
    Poco::Checksum checker(Poco::Checksum::TYPE_ADLER32);
    checker.update(pMessage->body.data(), pMessage->body.size());
    if (pMessage->checkSum != checker.checksum())
        throw Exception("Message check sum error");

    if (pMessage->typeName.length() == 0)
        throw Exception("Message type name error");

    return pMessage;
}

Base::ProtobufMsgPtr Base::Message::deserialize() {
    ProtobufMsgPtr pMessage;
    const google::protobuf::Descriptor* descriptor =
            google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(typeName);
    if (descriptor)
    {
        const google::protobuf::Message* prototype =
                google::protobuf::MessageFactory::generated_factory()->GetPrototype(descriptor);
        if (prototype)
        {
            pMessage.reset(prototype->New());
        }
    }
    return pMessage;
}

uint32_t Base::Message::serialize(char *buf, uint32_t bufSize) {
    if (length + sizeof(uint32_t) < bufSize)//缓存长度不够
        return 0;

    typeLen = typeName.length()+1;
    memcpy(buf, &length, sizeof(uint32_t));
    memcpy(buf + sizeof(uint32_t), &typeLen, sizeof(uint32_t));
    strncat(buf, typeName.c_str(), bufSize);

    // 计算消息头长度
    uint32_t len = 2 * sizeof(uint32_t) + typeLen;

    if (len + body.size() <= bufSize) {
        memcpy(buf+len, body.data(), body.size());
        len += body.size();
    }
    else {
        return 0;
    }

    //添加消息校验和
    Poco::Checksum checker(Poco::Checksum::TYPE_ADLER32);
    checker.update(body.data(), body.size());
    checkSum = checker.checksum();
    memcpy(buf+len, &checkSum, sizeof(uint32_t));
    len += sizeof(uint32_t);

    return len;
}

uint32_t Base::Message::size() {
    return length;
}

std::string &Base::Message::getTypeName() {
    return typeName;
}

