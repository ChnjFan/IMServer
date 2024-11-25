//
// Created by fan on 24-11-17.
//

#include <cstring>
#include "PduHeader.h"

Common::PduHeader::PduHeader(PDU_HEADER_DATA& data) : data(data) {

}

Common::PduHeader::PduHeader() {
    memset(&data, 0, sizeof(data));
}

uint32_t Common::PduHeader::getLength() const {
    return data.length;
}

void Common::PduHeader::setLength(uint32_t length) {
    data.length = length;
}

uint32_t Common::PduHeader::getMsgType() const {
    return data.msgType;
}

void Common::PduHeader::setMsgType(uint32_t msgType) {
    data.msgType = msgType;
}

uint32_t Common::PduHeader::getMsgSeq() const {
    return data.msgSeq;
}

void Common::PduHeader::setMsgSeq(uint32_t msgSeq) {
    data.msgSeq = msgSeq;
}

std::string Common::PduHeader::getUuid() const {
    return std::string(data.uuid);
}

void Common::PduHeader::setUuid(std::string uuid) {
    strncpy(data.uuid, uuid.c_str(), PDU_HEADER_UUID_LEN);
}

uint32_t Common::PduHeader::getPduHeaderLen() {
    return 3 * sizeof(uint32_t);
}

uint32_t Common::PduHeader::serialize(char *buf, uint32_t bufSize) {
    if (bufSize < getPduHeaderLen())
        return 0;

    memcpy(buf, &data, sizeof(data));
    return getPduHeaderLen();
}


