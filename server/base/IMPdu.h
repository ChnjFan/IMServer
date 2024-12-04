//
// Created by fan on 24-11-17.
//

#ifndef IMSERVER_IMPDU_H
#define IMSERVER_IMPDU_H

#include <memory>
#include "PduHeader.h"
#include "ByteStream.h"

namespace Base {

class IMPdu {
public:
    IMPdu();

    static std::shared_ptr<IMPdu> readPdu(Base::ByteStream &data);

    void setImPdu(PduHeader& header, Base::ByteStream& body);

    uint32_t serialize(char *buf, uint32_t bufSize);

    uint32_t getMsgType() const;

    uint32_t getMsgSeq() const;

    std::string getUuid() const;

    ByteStream& getMsgBody();

    uint32_t size();

private:
    void readHeader(Base::ByteStream &data);

    void readBody(Base::ByteStream &data, uint32_t size);

private:
    static constexpr uint32_t DEFAULT_BODY_LEN = 1024;
    PduHeader header;
    Base::ByteStream body;
};

}
#endif //IMSERVER_IMPDU_H
