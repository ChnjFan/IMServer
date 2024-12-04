//
// Created by fan on 24-11-17.
//

#include <cstring>
#include "PduHeader.h"

Base::PduHeader::PduHeader(PDU_HEADER_DATA& data) : data(data) {

}

Base::PduHeader::PduHeader() {
    memset(&data, 0, sizeof(data));
}

uint32_t Base::PduHeader::getLength() const {
    return data.length;
}

void Base::PduHeader::setLength(uint32_t length) {
    data.length = length;
}

uint32_t Base::PduHeader::getMsgType() const {
    return data.msgType;
}

void Base::PduHeader::setMsgType(uint32_t msgType) {
    data.msgType = msgType;
}

uint32_t Base::PduHeader::getMsgSeq() const {
    return data.msgSeq;
}

void Base::PduHeader::setMsgSeq(uint32_t msgSeq) {
    data.msgSeq = msgSeq;
}

std::string Base::PduHeader::getUuid() const {
    return std::string(data.uuid);
}

void Base::PduHeader::setUuid(std::string uuid) {
    strncpy(data.uuid, uuid.c_str(), PDU_HEADER_UUID_LEN);
}

uint32_t Base::PduHeader::getPduHeaderLen() {
    return 3 * sizeof(uint32_t);
}

uint32_t Base::PduHeader::serialize(char *buf, uint32_t bufSize) {
    if (bufSize < getPduHeaderLen())
        return 0;

    memcpy(buf, &data, sizeof(data));
    return getPduHeaderLen();
}

Base::PDU_HEADER_DATA &Base::PduHeader::getHeaderData() {
    return data;
}

void Base::PduHeader::operator=(Base::PduHeader &other) {
    memcpy(&data, &other.getHeaderData(), sizeof(PDU_HEADER_DATA));
}

