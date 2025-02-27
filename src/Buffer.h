#ifndef IMSERVER_BUFFER_H
#define IMSERVER_BUFFER_H

#include <vector>
#include <cstdint>
#include <cassert>
#include <cstring>
#include "Poco/ByteOrder.h"

namespace Base {

class Buffer {
public:
    static constexpr std::size_t DEFAULT_BUFFER_SIZE = 1024;

    explicit Buffer(std::size_t capacity = DEFAULT_BUFFER_SIZE) : buffer(capacity), readerIndex(0), writerIndex(0) { }

    std::size_t readableBytes() const { return writerIndex - readerIndex; }

    std::size_t writableBytes() const {return buffer.size() - writerIndex; }

    char* beginWrite() { return begin() + writerIndex; }

    const char* beginWrite() const { return begin() + writerIndex; }

    const char* peek() const { return begin(); }

    int32_t peekInt32() const {
        assert(readableBytes() >= sizeof(int32_t));
        int32_t num = 0;
        memcpy(&num, peek(), sizeof(int32_t));
    }

    int32_t readInt32() {
        int32_t result = peekInt32();
        retrieveInt32();
        return result;
    }

    void append(const char* data, size_t size) {
        ensureWritableBytes(size);
        std::copy(data, data + size, beginWrite());
        writerIndex += size;
    }

    void append(void* data, size_t size) {
        append(static_cast<const char*>(data), size);
    }

    void appendInt32(int32_t num) {
        int32_t netNum = Poco::ByteOrder::toNetwork(num);
        append(&netNum, sizeof(int32_t));
    }

private:
    char* begin() { return &*buffer.begin(); }

    const char* begin() const { return &*buffer.begin(); }


    /* 清空数组 */
    void retrieveAll() {
        readerIndex = 0;
        writerIndex = 0;
    }

    void retrieve(size_t size) {
        assert(size <= readableBytes());
        if (size < readableBytes()) {
            readerIndex += size;
        }
        else {
            retrieveAll();
        }
    }

    void retrieveInt32() {
        retrieve(sizeof(int32_t));
    }

    void ensureWritableBytes(size_t size) {
        if (size > writableBytes()) {
            // 将数据拷贝到数组开始
            size_t readable = readableBytes();
            std::copy(begin()+readerIndex, begin()+writerIndex, begin());
            readerIndex = 0;
            writerIndex = readable;
            buffer.resize(size);
        }
        assert(writableBytes() >= size);
    }


private:
    std::vector<char> buffer;
    std::size_t readerIndex;
    std::size_t writerIndex;
};

};

#endif //IMSERVER_BUFFER_H
