/**
 * @file ByteStream.h
 * @brief Byte stream buffer
 * @author ChnjFan
 * @date 2024-12-04
 * @copyright Copyright (c) 2024 ChnjFan. All rights reserved.
 */
#ifndef IMSERVER_BYTESTREAM_H
#define IMSERVER_BYTESTREAM_H

#include <vector>
#include <string>

namespace Base {

/**
 * @class ByteStream
 * @brief For TCP connections to send and receive data.
 * Byte stream buffer is used to save the data without providing message encoding and decoding.
 */
class ByteStream {
public:
    explicit ByteStream(std::size_t size=DEFAULT_BYTESTREAM_SIZE);
    ByteStream(ByteStream&& other) noexcept;

    ByteStream(ByteStream& other) = default;
    ~ByteStream();

    void write(char* data, uint32_t size);
    void write(ByteStream &data, uint32_t size);

    std::vector<char>& getBuffer();
    const char* data();

    ByteStream read(uint32_t size);
    void read(uint32_t size, std::vector<char>& buf);
    void peek(uint32_t size, std::vector<char>& buf);

    /**
     * @brief 查看缓冲区前4字节数据
     */
    uint32_t peekUint32();
    /**
     * @brief 读取缓冲区前4字节数据，读完后删除缓冲区内容
     */
    uint32_t readUint32();

    std::string readString(uint32_t size);

    uint32_t size();

    bool empty();
    void clear();

    void operator=(ByteStream& other);

private:
    /**
     * @brief Byte stream buffer default size
     */
    static constexpr int DEFAULT_BYTESTREAM_SIZE = 10;
    std::vector<char> buffer;
};

}

#endif //IMSERVER_BYTESTREAM_H
