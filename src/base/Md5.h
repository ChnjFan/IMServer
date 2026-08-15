//
// Created by Fan on 2026/7/3.
//

#ifndef IMSERVER_MD5_H
#define IMSERVER_MD5_H

#include <string>

#include <openssl/evp.h>

class Md5 {
public:
    Md5();
    ~Md5();
    Md5(const Md5&) = delete;
    Md5& operator=(const Md5&) = delete;

    void update(const void* data, size_t len);
    void update(const std::string& data);
    [[nodiscard]] std::string finalize() const;
    void reset();

    static std::string compute(const void* data, size_t len);
    static std::string compute(const std::string& data);
    static bool verify(const void* data, size_t len, const std::string& expectedHex);

private:
    EVP_MD_CTX* ctx_;
};

#endif //IMSERVER_MD5_H
