//
// Created by fan on 24-11-17.
//

#include "PduHeader.h"

uint32_t PduHeader::getLength() const {
    return length;
}

void PduHeader::setLength(uint32_t length) {
    PduHeader::length = length;
}

uint32_t PduHeader::getMsgType() const {
    return msgType;
}

void PduHeader::setMsgType(uint32_t msgType) {
    PduHeader::msgType = msgType;
}

uint32_t PduHeader::getMsgSeq() const {
    return msgSeq;
}

void PduHeader::setMsgSeq(uint32_t msgSeq) {
    PduHeader::msgSeq = msgSeq;
}

uint32_t PduHeader::getPduHeaderLen() {
    return 3 * sizeof(uint32_t);
}
