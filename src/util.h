//
// Created by fan on 25-2-23.
//

#ifndef IMSERVER_UTIL_H
#define IMSERVER_UTIL_H

#include <string>
#include <Poco/SHA1Engine.h>

namespace Base {
    /* 根据登录请求生成token */
    std::string generateToken(const std::string& tokenBase, const std::string& secretKey) {
        // 使用SHA1哈希算法生成签名
        Poco::SHA1Engine sha1;
        sha1.update(tokenBase + secretKey); // 添加密钥以增强安全性
        // 获取哈希值
        Poco::DigestEngine::Digest digest = sha1.digest();
        // 将哈希值编码为十六进制字符串
        std::string token = tokenBase + "." + Poco::DigestEngine::digestToHex(digest);
        return token;
    }
}

#endif //IMSERVER_UTIL_H
