//
// Created by fan on 24-11-17.
//

#include <cstring>
#include "PduHeader.h"

PduHeader::PduHeader(PDU_HEADER_DATA& data) : data(data) {

}

PduHeader::PduHeader() {
    memset(&data, 0, sizeof(data));
}

uint32_t PduHeader::getLength() const {
    return data.length;
}

void PduHeader::setLength(uint32_t length) {
    data.length = length;
}

uint32_t PduHeader::getMsgType() const {
    return data.msgType;
}

void PduHeader::setMsgType(uint32_t msgType) {
    data.msgType = msgType;
}

uint32_t PduHeader::getMsgSeq() const {
    return data.msgSeq;
}

void PduHeader::setMsgSeq(uint32_t msgSeq) {
    data.msgSeq = msgSeq;
}

uint32_t PduHeader::getPduHeaderLen() {
    return 3 * sizeof(uint32_t);
}

uint32_t PduHeader::serialize(char *buf, uint32_t bufSize) {
    if (bufSize < getPduHeaderLen())
        return 0;

    memcpy(buf, &data, sizeof(data));
    return getPduHeaderLen();
}
