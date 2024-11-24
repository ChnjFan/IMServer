//
// Created by fan on 24-11-17.
//

#ifndef IMSERVER_IMPDU_H
#define IMSERVER_IMPDU_H

#include <memory>
#include "PduHeader.h"
#include "ByteStream.h"

class IMPdu {
public:
    IMPdu();

    static std::shared_ptr<IMPdu> readPdu(ByteStream &data);

    uint32_t serialize(char* buf, uint32_t bufSize);

    uint32_t getMsgType() const;
    std::string getUuid() const;

    uint32_t size();

private:
    void readHeader(ByteStream &data);
    void readBody(ByteStream &data, uint32_t size);

private:
    static constexpr uint32_t DEFAULT_BODY_LEN = 1024;
    PduHeader header;
    ByteStream body;
};


#endif //IMSERVER_IMPDU_H
