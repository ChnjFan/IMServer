//
// Created by fan on 24-11-17.
//

#include "ByteStream.h"

Common::ByteStream::ByteStream(std::size_t size) {
    buffer.reserve(size);
}

void Common::ByteStream::write(char *data, uint32_t size) {
    std::copy(data, data + size, std::back_inserter(buffer));
}

void Common::ByteStream::write(ByteStream &data, uint32_t size) {
    std::vector<char> tmp = data.read(size);
    std::copy(tmp.begin(), tmp.end(), std::back_inserter(buffer));
}

const char* Common::ByteStream::data() {
    return buffer.data();
}

std::vector<char> Common::ByteStream::read(uint32_t size) {
    std::vector<char> data;
    if (size <= buffer.size()) {
        data.insert(data.end(), buffer.begin(), buffer.begin() + size);
        buffer.erase(buffer.begin(), buffer.begin() + size);
    }
    return data;
}

std::vector<char> Common::ByteStream::peek(uint32_t size) {
    std::vector<char> data;
    if (size <= buffer.size()) {
        data.insert(data.end(), buffer.begin(), buffer.begin() + size);
    }
    return data;
}

uint32_t Common::ByteStream::peekUint32() {
    std::vector<char> buf = peek(sizeof(uint32_t));
    if (buf.size() != sizeof(uint32_t))
        return 0;

    // 网络序转换
    uint32_t data = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
    return data;
}

uint32_t Common::ByteStream::readUint32() {
    std::vector<char> buf = read(sizeof(uint32_t));
    if (buf.size() != sizeof(uint32_t))
        return 0;

    // 网络序转换
    uint32_t data = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
    return data;
}

uint32_t Common::ByteStream::size() {
    return buffer.size();
}

bool Common::ByteStream::empty() {
    return buffer.empty();
}

void Common::ByteStream::clear() {
    buffer.clear();
}
