//
// Created by fan on 24-11-17.
//

#include <memory>
#include "IMPdu.h"

IMPdu::IMPdu(): header(0, 0, 0), body(DEFAULT_BODY_LEN) {

}

std::shared_ptr<IMPdu> IMPdu::readPdu(ByteStream &data) {
    uint32_t len = data.peekUint32();

    if (len + PduHeader::getPduHeaderLen() > data.size())
        return nullptr;

    std::shared_ptr<IMPdu> pImPdu = std::make_shared<IMPdu>();
    pImPdu->readHeader(data);
    pImPdu->readBody(data, len);

    return pImPdu;
}

void IMPdu::readHeader(ByteStream &data) {
    uint32_t len = data.readUint32();
    uint32_t type = data.readUint32();
    uint32_t seq = data.readUint32();

    header.setLength(len);
    header.setMsgType(type);
    header.setMsgSeq(seq);
}

void IMPdu::readBody(ByteStream &data, uint32_t size) {
    body.write(data, size);
}
