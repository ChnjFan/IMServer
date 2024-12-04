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
    std::vector<char> tmp(size);
    data.read(size, tmp);
    std::copy(tmp.begin(), tmp.end(), std::back_inserter(buffer));
}

std::vector<char> &Base::ByteStream::getBuffer() {
    return buffer;
}

const char* Base::ByteStream::data() {
    return buffer.data();
}

Base::ByteStream Base::ByteStream::read(uint32_t size) {
    Base::ByteStream data;
    std::copy(buffer.begin(), buffer.begin() + size, std::back_inserter(data.buffer));
    buffer.erase(buffer.begin(), buffer.begin() + size);

    return data;
}

void Base::ByteStream::read(uint32_t size, std::vector<char>& buf) {
    if (size <= buffer.size()) {
        buf.insert(buf.end(), buffer.begin(), buffer.begin() + size);
        buffer.erase(buffer.begin(), buffer.begin() + size);
    }
    return;
}

void Base::ByteStream::peek(uint32_t size, std::vector<char>& buf) {
    if (size <= buffer.size()) {
        buf.insert(buf.end(), buffer.begin(), buffer.begin() + size);
    }
    return;
}

uint32_t Base::ByteStream::peekUint32() {
    std::vector<char> buf(sizeof(uint32_t));
    peek(sizeof(uint32_t), buf);
    if (buf.size() != sizeof(uint32_t))
        return 0;

    // 网络序转换
    uint32_t data = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
    return data;
}

uint32_t Base::ByteStream::readUint32() {
    std::vector<char> buf(sizeof(uint32_t));
    read(sizeof(uint32_t), buf);
    if (buf.size() != sizeof(uint32_t))
        return 0;

    // 网络序转换
    uint32_t data = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
    return data;
}

std::string Base::ByteStream::readString(uint32_t size) {
    if (buffer[size] != '\0')
        return "";
    std::string data(buffer.begin(), buffer.begin() + size);
    buffer.erase(buffer.begin(), buffer.begin() + size);
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
