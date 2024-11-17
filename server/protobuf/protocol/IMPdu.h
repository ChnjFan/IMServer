//
// Created by fan on 24-11-17.
//

#ifndef IMSERVER_IMPDU_H
#define IMSERVER_IMPDU_H

#include "PduHeader.h"
#include "ByteStream.h"

class IMPdu {
public:
    IMPdu();

    static std::shared_ptr<IMPdu> readPdu(ByteStream &data);

private:
    void readHeader(ByteStream &data);
    void readBody(ByteStream &data, uint32_t size);

private:
    static constexpr uint32_t DEFAULT_BODY_LEN = 1024;
    PduHeader header;
    ByteStream body;
};


#endif //IMSERVER_IMPDU_H
