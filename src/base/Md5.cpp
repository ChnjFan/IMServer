//
// Created by Fan on 2026/7/3.
//

#include "Md5.h"

#include <iomanip>
#include <sstream>

#include <openssl/evp.h>
#include <openssl/md5.h>

Md5::Md5() : ctx_(EVP_MD_CTX_new()) {
    reset();
}

Md5::~Md5() {
    EVP_MD_CTX_free(ctx_);
}

void Md5::update(const void* data, size_t len) {
    EVP_DigestUpdate(ctx_, data, len);
}

void Md5::update(const std::string& data) {
    update(data.data(), data.size());
}

std::string Md5::finalize() const {
    unsigned char buf[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    // finalize 需要非 const ctx，但 EVP_DigestFinal_ex 不修改算法状态语义
    // 这里复制一份 ctx 来 final，保持 finalize() const 语义
    EVP_MD_CTX* tmp = EVP_MD_CTX_new();
    EVP_MD_CTX_copy(tmp, ctx_);
    EVP_DigestFinal_ex(tmp, buf, &len);
    EVP_MD_CTX_free(tmp);

    std::stringstream ss;
    for (unsigned int i = 0; i < len; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(buf[i]);
    }
    return ss.str();
}

void Md5::reset() {
    EVP_DigestInit_ex(ctx_, EVP_md5(), nullptr);
}

std::string Md5::compute(const void* data, size_t len) {
    Md5 md5;
    md5.update(data, len);
    return md5.finalize();
}

std::string Md5::compute(const std::string& data) {
    return compute(data.data(), data.size());
}

bool Md5::verify(const void* data, size_t len, const std::string& expectedHex) {
    std::string actual = compute(data, len);
    if (actual.size() != expectedHex.size()) return false;
    // 大小写不敏感比较
    for (size_t i = 0; i < actual.size(); ++i) {
        if (std::tolower(actual[i]) != std::tolower(expectedHex[i])) return false;
    }
    return true;
}
