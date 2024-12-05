/**
 * @file Exception.h
 * @brief 异常管理
 * @author ChnjFan
 * @date 2024-12-04
 * @copyright Copyright (c) 2024 ChnjFan. All rights reserved.
 */

#ifndef IMSERVER_EXCEPTION_H
#define IMSERVER_EXCEPTION_H

#include <string>
#include <exception>

namespace Base {

/**
 * @class ByteStream
 * @brief For TCP connections to send and receive data.
 * Byte stream buffer is used to save the data without providing message encoding and decoding.
 */
class Exception : public std::exception {
public:
    explicit Exception(const std::string what);

    ~Exception() noexcept override = default;

    const char *what() const noexcept override;

private:
    std::string message;
};

}

#endif //IMSERVER_EXCEPTION_H
