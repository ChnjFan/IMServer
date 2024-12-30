//
// Created by fan on 24-11-17.
//

#include <memory>
#include <cstring>
#include "Message.h"
#include "Exception.h"
#include "Poco/ByteOrder.h"
#include "Poco/Checksum.h"

Base::Message::Message(): length(0), typeLen(0), typeName(""), checkSum(0), body(DEFAULT_BODY_LEN) { }

Base::Message::Message(ByteBuffer &body, std::string typeName)
                    : length(DEFAULT_HEADER_LEN + typeName.length() + 1 + body.size())
                    , typeLen(typeName.length() + 1)
                    , typeName(typeName)
                    , checkSum(0)
                    , body(body) {
    Poco::Checksum checker(Poco::Checksum::TYPE_ADLER32);
    checker.update((const char*)body.begin(), body.size());
    checkSum = checker.checksum();
}

Base::MessagePtr Base::Message::getMessage(ByteBuffer &data) {
    Base::MessagePtr pMessage = std::make_shared<Message>();
    uint32_t readCount = 0;
    unsigned char *pBuffer = data.begin();

    // 读取消息头
    std::copy(pBuffer, pBuffer + sizeof(pMessage->length), &pMessage->length);
    pBuffer += sizeof(pMessage->length);
    readCount += sizeof(pMessage->length);
    pMessage->length = Poco::ByteOrder::fromNetwork(pMessage->length);
    // 缓存内容不能组成一个消息长度
    if (pMessage->length < data.sizeBytes())
        return nullptr;
    // 消息不能超过最大消息长度MAX_MESSAGE_LEN，超长属于异常，需要关闭连接让客户端重连
    if (pMessage->length > MAX_MESSAGE_LEN)
        throw Base::Exception("Message length error, len = " + pMessage->length);

    // 类型长度
    std::copy(pBuffer, pBuffer + sizeof(pMessage->typeLen), &pMessage->typeLen);
    pBuffer += sizeof(pMessage->typeLen);
    readCount += sizeof(pMessage->typeLen);
    pMessage->typeLen = Poco::ByteOrder::fromNetwork(pMessage->typeLen);
    if (pBuffer[pMessage->typeLen] != '\0' || pMessage->typeLen == 0)
        throw Base::Exception("Message typeName error, typeLen = " + std::to_string(pMessage->typeLen) + " typeName = " + (char*)pBuffer);

    // 类型名称
    pMessage->typeName = (char*)pBuffer;
    pBuffer += pMessage->typeLen + 1;
    readCount += pMessage->typeLen + 1;

    // 消息体
    int bodySize = pMessage->length - readCount - sizeof(pMessage->checkSum);
    pMessage->body.resize(bodySize);
    std::copy(pBuffer, pBuffer + bodySize, pMessage->body.begin());
    pBuffer += bodySize;
    readCount += bodySize;

    // 计算校验和，校验失败返回空，此时该消息也从缓冲区中读完
    std::copy(pBuffer, pBuffer + sizeof(pMessage->checkSum), &pMessage->checkSum);
    pMessage->checkSum = Poco::ByteOrder::fromNetwork(pMessage->checkSum);
    Poco::Checksum checker(Poco::Checksum::TYPE_ADLER32);
    checker.update((const char*)pMessage->body.begin(), pMessage->body.size());
    if (pMessage->checkSum != checker.checksum())
        throw Exception("Message check sum error");

    if (readCount < data.size())
    {
        ByteBuffer tempBuffer(data.size() - readCount);
        std::copy(data.begin() + readCount, data.end(), tempBuffer.begin());
        data.swap(tempBuffer);
    }

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

uint32_t Base::Message::serialize(unsigned char *buf, uint32_t bufSize) {
    if (length + sizeof(uint32_t) < bufSize)//缓存长度不够
        return 0;

    typeLen = typeName.length()+1;
    memcpy(buf, &length, sizeof(uint32_t));
    memcpy(buf + sizeof(uint32_t), &typeLen, sizeof(uint32_t));
    strncat((char*)buf, typeName.c_str(), bufSize);

    // 计算消息头长度
    uint32_t len = 2 * sizeof(uint32_t) + typeLen;

    if (len + body.size() <= bufSize) {
        memcpy(buf+len, body.begin(), body.size());
        len += body.size();
    }
    else {
        return 0;
    }

    //添加消息校验和
    Poco::Checksum checker(Poco::Checksum::TYPE_ADLER32);
    checker.update((const char*)body.begin(), body.size());
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

