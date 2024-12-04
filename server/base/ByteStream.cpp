//
// Created by fan on 24-11-17.
//

#include "ByteStream.h"

Base::ByteStream::ByteStream(std::size_t size) {
    buffer.reserve(size);
}

void Base::ByteStream::write(char *data, uint32_t size) {
    std::copy(data, data + size, std::back_inserter(buffer));
}

void Base::ByteStream::write(ByteStream &data, uint32_t size) {
    std::vector<char> tmp = data.read(size);
    std::copy(tmp.begin(), tmp.end(), std::back_inserter(buffer));
}

std::vector<char> &Base::ByteStream::getBuffer() {
    return buffer;
}

const char* Base::ByteStream::data() {
    return buffer.data();
}

std::vector<char> Base::ByteStream::read(uint32_t size) {
    std::vector<char> data;
    if (size <= buffer.size()) {
        data.insert(data.end(), buffer.begin(), buffer.begin() + size);
        buffer.erase(buffer.begin(), buffer.begin() + size);
    }
    return data;
}

std::vector<char> Base::ByteStream::peek(uint32_t size) {
    std::vector<char> data;
    if (size <= buffer.size()) {
        data.insert(data.end(), buffer.begin(), buffer.begin() + size);
    }
    return data;
}

uint32_t Base::ByteStream::peekUint32() {
    std::vector<char> buf = peek(sizeof(uint32_t));
    if (buf.size() != sizeof(uint32_t))
        return 0;

    // 网络序转换
    uint32_t data = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
    return data;
}

uint32_t Base::ByteStream::readUint32() {
    std::vector<char> buf = read(sizeof(uint32_t));
    if (buf.size() != sizeof(uint32_t))
        return 0;

    // 网络序转换
    uint32_t data = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
    return data;
}

uint32_t Base::ByteStream::size() {
    return buffer.size();
}

bool Base::ByteStream::empty() {
    return buffer.empty();
}

void Base::ByteStream::clear() {
    buffer.clear();
}

void Base::ByteStream::operator=(Base::ByteStream &other) {
    std::copy(other.buffer.begin(), other.buffer.end(), std::back_inserter(buffer));
}
