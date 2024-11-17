//
// Created by fan on 24-11-17.
//

#ifndef IMSERVER_PDUHEADER_H
#define IMSERVER_PDUHEADER_H

#include <cstdint>

class PduHeader {
public:
    PduHeader(uint32_t len, uint32_t type, uint32_t seq) : length(len), msgType(type), msgSeq(seq) { }

    PduHeader();

    uint32_t getLength() const;

    void setLength(uint32_t length);

    uint32_t getMsgType() const;

    void setMsgType(uint32_t msgType);

    uint32_t getMsgSeq() const;

    void setMsgSeq(uint32_t msgSeq);

    static uint32_t getPduHeaderLen();

private:
    uint32_t length;
    uint32_t msgType;
    uint32_t msgSeq;
};

#endif //IMSERVER_PDUHEADER_H
