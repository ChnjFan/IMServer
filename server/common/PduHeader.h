//
// Created by fan on 24-11-17.
//

#ifndef IMSERVER_PDUHEADER_H
#define IMSERVER_PDUHEADER_H

#include <cstdint>

typedef struct {
    uint32_t length;
    uint32_t msgType;
    uint32_t msgSeq;
}PDU_HEADER_DATA;

class PduHeader {
public:
    explicit PduHeader(PDU_HEADER_DATA& data);

    PduHeader();

    uint32_t getLength() const;

    void setLength(uint32_t length);

    uint32_t getMsgType() const;

    void setMsgType(uint32_t msgType);

    uint32_t getMsgSeq() const;

    void setMsgSeq(uint32_t msgSeq);

    static uint32_t getPduHeaderLen();

    uint32_t serialize(char *buf, uint32_t bufSize);

private:
    PDU_HEADER_DATA data;
};

#endif //IMSERVER_PDUHEADER_H
