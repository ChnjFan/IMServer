//
// Created by Fan on 2026/7/2.
//

#include "AuthMiddleware.h"

#include <iostream>

#include "const.h"
#include "StatusGrpcClient.h"

AuthMiddleware::AuthResult AuthMiddleware::authenticate(const http::request<http::dynamic_body>& req) {
    const auto token = extractBearerToken(req);
    const auto uid = extractHeaderInt(req, "X-User-Id");
    const auto convId = extractHeaderString(req, "X-Conversation-Id");

    if (token.empty() || uid < 0) {
        std::cout << "token null or uid " << uid << " error." << std::endl;
        return { static_cast<int>(ErrorCodes::RESOURCE_AUTH_FAILED ), -1, "" };
    }

    if (!verifyTokenFromStatusServer(uid, token)) {
        return { static_cast<int>(ErrorCodes::RESOURCE_AUTH_FAILED ), -1, "" };
    }

    // 上传头像等功能不需要校验会话 ID
    if (!convId.empty() && !isConversationMember(uid, convId)) {
        return { static_cast<int>(ErrorCodes::RESOURCE_ACCESS_DENIED ), uid, convId };
    }

    return { 0, uid, convId };
}

std::string AuthMiddleware::extractBearerToken(const http::request<http::dynamic_body>& req) {
    const auto authHeader = std::string(req[http::field::authorization]);
    if (authHeader.empty()) return "";

    // Expect "Bearer <token>"
    if (authHeader.substr(0, 7) == "Bearer ") {
        return authHeader.substr(7);
    }
    return "";
}

int AuthMiddleware::extractHeaderInt(const http::request<http::dynamic_body>& req,
                                      const std::string& name) {
    const auto it = req.find(name);
    if (it == req.end()) return -1;
    try {
        return std::stoi(std::string(it->value()));
    } catch (...) {
        return -1;
    }
}

std::string AuthMiddleware::extractHeaderString(const http::request<http::dynamic_body>& req,
                                                const std::string& name) {
    const auto it = req.find(name);
    if (it == req.end()) return "";
    return std::string(it->value());
}

bool AuthMiddleware::verifyTokenFromStatusServer(const int uid, const std::string& token) {
    const auto response = StatusGrpcClient::getInstance()->VerifyToken(uid, token);
    if (response.error() != static_cast<int32_t>(ErrorCodes::SUCCESS)) {
        return false;
    }
    return response.error() == static_cast<int32_t>(ErrorCodes::SUCCESS);
}

bool AuthMiddleware::isConversationMember(const int uid, const std::string& convId) {
    // conv_id format: "c2c_{uid1}_{uid2}" or "group_{gid}"
    // For c2c: parse uids from conv_id
    if (convId.substr(0, 4) == "c2c_") {
        // Format: c2c_{minUid}_{maxUid}
        const size_t first = convId.find('_', 4);
        if (first == std::string::npos) return false;
        const size_t second = convId.find('_', first + 1);
        if (second == std::string::npos) return false;

        try {
            const int uid1 = std::stoi(convId.substr(first + 1, second - first - 1));
            const int uid2 = std::stoi(convId.substr(second + 1));
            return (uid == uid1 || uid == uid2);
        } catch (...) {
            return false;
        }
    }

    // For group chat, would need to query group member table
    // For now, allow access (group membership check is a TODO)
    return true;
}
