//
// Created by fan on 24-11-17.
//

#ifndef IMSERVER_BYTESTREAM_H
#define IMSERVER_BYTESTREAM_H

#include <vector>
#include <cstdint>

namespace Common {

class ByteStream {
public:
    explicit ByteStream(std::size_t size);

    void write(char* data, uint32_t size);
    void write(ByteStream &data, uint32_t size);

    const char* data();

    std::vector<char> read(uint32_t size);
    std::vector<char> peek(uint32_t size);

    uint32_t peekUint32();
    uint32_t readUint32();

    uint32_t size();

    bool empty();
    void clear();

private:
    std::vector<char> buffer;
};

}

#endif //IMSERVER_BYTESTREAM_H
