//
// Created by fan on 24-11-17.
//

#include <memory>
#include <cstring>
#include "IMPdu.h"

Common::IMPdu::IMPdu(): header(), body(DEFAULT_BODY_LEN) {

}

std::shared_ptr<Common::IMPdu> Common::IMPdu::readPdu(Common::ByteStream &data) {
    uint32_t len = data.peekUint32();

    if (len + PduHeader::getPduHeaderLen() > static_cast<uint32_t>(data.size()))
        return nullptr;

    std::shared_ptr<IMPdu> pImPdu = std::make_shared<IMPdu>();
    pImPdu->readHeader(data);
    pImPdu->readBody(data, len);

    return pImPdu;
}

void Common::IMPdu::setImPdu(Common::PduHeader &header, Common::ByteStream &body) {
    this->header = header;
    this->body = body;
}

uint32_t Common::IMPdu::serialize(char *buf, uint32_t bufSize) {
    if (header.getLength() != body.size())//帧头设置的长度与消息体长度不一致
        return 0;

    uint32_t len = header.serialize(buf, bufSize);

    if (len + body.size() <= bufSize) {
        memcpy(buf+len, body.data(), body.size());
        len += body.size();
    }

    return len;
}

uint32_t Common::IMPdu::getMsgType() const {
    return header.getMsgType();
}

uint32_t Common::IMPdu::getMsgSeq() const {
    return header.getMsgSeq();
}

std::string Common::IMPdu::getUuid() const {
    return header.getUuid();
}

Common::ByteStream& Common::IMPdu::getMsgBody() {
    return body;
}

void Common::IMPdu::readHeader(Common::ByteStream &data) {
    header.setLength(data.readUint32());
    header.setMsgType(data.readUint32());
    header.setMsgSeq(data.readUint32());
}

void Common::IMPdu::readBody(Common::ByteStream &data, uint32_t size) {
    body.write(data, size);
}

uint32_t Common::IMPdu::size() {
    return PduHeader::getPduHeaderLen() + body.size();
}

