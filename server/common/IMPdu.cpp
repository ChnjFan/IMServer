//
// Created by fan on 24-11-17.
//

#include <memory>
#include <cstring>
#include "IMPdu.h"

IMPdu::IMPdu(): header(), body(DEFAULT_BODY_LEN) {

}

std::shared_ptr<IMPdu> IMPdu::readPdu(ByteStream &data) {
    uint32_t len = data.peekUint32();

    if (len + PduHeader::getPduHeaderLen() > static_cast<uint32_t>(data.size()))
        return nullptr;

    std::shared_ptr<IMPdu> pImPdu = std::make_shared<IMPdu>();
    pImPdu->readHeader(data);
    pImPdu->readBody(data, len);

    return pImPdu;
}

uint32_t IMPdu::serialize(char *buf, uint32_t bufSize) {
    if (header.getLength() != body.size())//帧头设置的长度与消息体长度不一致
        return 0;

    uint32_t len = header.serialize(buf, bufSize);

    if (len + body.size() <= bufSize) {
        memcpy(buf+len, body.data(), body.size());
        len += body.size();
    }

    return len;
}

uint32_t IMPdu::getMsgType() const {
    return header.getMsgType();
}

void IMPdu::readHeader(ByteStream &data) {
    header.setLength(data.readUint32());
    header.setMsgType(data.readUint32());
    header.setMsgSeq(data.readUint32());
}

void IMPdu::readBody(ByteStream &data, uint32_t size) {
    body.write(data, size);
}

uint32_t IMPdu::size() {
    return PduHeader::getPduHeaderLen() + body.size();
}

