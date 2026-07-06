//
// Created by Fan on 2026/7/2.
//

#ifndef IMSERVER_AUTHMIDDLEWARE_H
#define IMSERVER_AUTHMIDDLEWARE_H

#include <string>

#include "const.h"

namespace http = boost::beast::http;

class AuthMiddleware {
public:
    struct AuthResult {
        int error = 0;
        int uid = -1;
        std::string convId;
    };

    static AuthResult authenticate(const http::request<http::dynamic_body>& req);

    // 供 ChunkUploadHandler 等读取自定义 header
    static int extractHeaderInt(const http::request<http::dynamic_body>& req,
                                const std::string& name);
    static std::string extractHeaderString(const http::request<http::dynamic_body>& req,
                                           const std::string& name);

private:
    static std::string extractBearerToken(const http::request<http::dynamic_body>& req);
    static bool verifyTokenFromStatusServer(int uid, const std::string& token);
    static bool isConversationMember(int uid, const std::string& convId);
};


#endif //IMSERVER_AUTHMIDDLEWARE_H
