#include "AuthCenter.h"

#include <iostream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <cstring>

namespace gateway {

AuthCenter::AuthCenter() {}

void AuthCenter::initialize(const AuthConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    config_ = config;
    initialized_ = true;
    
    std::cout << "AuthCenter initialized successfully!" << std::endl;
    std::cout << "Authentication: " << (config_.enable_authentication ? "enabled" : "disabled") << std::endl;
    std::cout << "JWT Secret: " << config_.jwt_secret << std::endl;
    std::cout << "JWT Expire Time: " << config_.jwt_expire_time << " seconds" << std::endl;
}

bool AuthCenter::validateToken(const std::string& token) {
    if (!initialized_ || !config_.enable_authentication) {
        return true; // 未初始化或未启用认证，默认允许
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 检查是否在黑名单中
    if (invalid_tokens_.find(token) != invalid_tokens_.end()) {
        return false;
    }
    
    std::unordered_map<std::string, std::string> payload;
    if (!parseJwtToken(token, payload)) {
        return false;
    }
    
    // 检查Token是否过期
    auto it = payload.find("exp");
    if (it == payload.end()) {
        return false;
    }
    
    try {
        uint64_t exp_time = std::stoull(it->second);
        uint64_t now = std::chrono::duration_cast<std::chrono::seconds>
            (std::chrono::system_clock::now().time_since_epoch()).count();
        
        return exp_time > now;
    } catch (...) {
        return false;
    }
}

std::string AuthCenter::generateToken(uint32_t user_id, const std::string& username) {
    if (!initialized_ || !config_.enable_authentication) {
        return "";
    }
    
    std::unordered_map<std::string, std::string> payload;
    payload["user_id"] = std::to_string(user_id);
    payload["username"] = username;
    payload["iat"] = std::to_string(std::chrono::duration_cast<std::chrono::seconds>
        (std::chrono::system_clock::now().time_since_epoch()).count());
    payload["exp"] = std::to_string(std::chrono::duration_cast<std::chrono::seconds>
        (std::chrono::system_clock::now().time_since_epoch() + std::chrono::seconds(config_.jwt_expire_time)).count());
    
    return generateJwtToken(payload);
}

uint32_t AuthCenter::getUserIdFromToken(const std::string& token) {
    if (!initialized_ || !config_.enable_authentication) {
        return 0;
    }
    
    std::unordered_map<std::string, std::string> payload;
    if (!parseJwtToken(token, payload)) {
        return 0;
    }
    
    auto it = payload.find("user_id");
    if (it == payload.end()) {
        return 0;
    }
    
    try {
        return std::stoul(it->second);
    } catch (...) {
        return 0;
    }
}

std::string AuthCenter::getUsernameFromToken(const std::string& token) {
    if (!initialized_ || !config_.enable_authentication) {
        return "";
    }
    
    std::unordered_map<std::string, std::string> payload;
    if (!parseJwtToken(token, payload)) {
        return "";
    }
    
    auto it = payload.find("username");
    if (it == payload.end()) {
        return "";
    }
    
    return it->second;
}

std::string AuthCenter::refreshToken(const std::string& old_token) {
    if (!initialized_ || !config_.enable_authentication) {
        return "";
    }
    
    if (!validateToken(old_token)) {
        return "";
    }
    
    uint32_t user_id = getUserIdFromToken(old_token);
    std::string username = getUsernameFromToken(old_token);
    
    if (user_id == 0 || username.empty()) {
        return "";
    }
    
    // 使旧Token失效
    invalidateToken(old_token);
    
    // 生成新Token
    return generateToken(user_id, username);
}

void AuthCenter::invalidateToken(const std::string& token) {
    if (!initialized_ || !config_.enable_authentication) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    invalid_tokens_[token] = true;
}

bool AuthCenter::checkPermission(uint32_t user_id, const std::string& permission) {
    if (!initialized_ || !config_.enable_authentication) {
        return true;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto user_it = user_permissions_.find(user_id);
    if (user_it == user_permissions_.end()) {
        return false;
    }
    
    auto perm_it = user_it->second.find(permission);
    return perm_it != user_it->second.end() && perm_it->second;
}

void AuthCenter::addPermission(uint32_t user_id, const std::string& permission) {
    if (!initialized_ || !config_.enable_authentication) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    user_permissions_[user_id][permission] = true;
}

void AuthCenter::removePermission(uint32_t user_id, const std::string& permission) {
    if (!initialized_ || !config_.enable_authentication) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    auto user_it = user_permissions_.find(user_id);
    if (user_it != user_permissions_.end()) {
        user_it->second.erase(permission);
    }
}

void AuthCenter::bindUserIdToConnection(uint32_t user_id, network::ConnectionId connection_id) {
    if (!initialized_ || !config_.enable_authentication) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 更新映射
    user_connection_map_[user_id] = connection_id;
    connection_user_map_[connection_id] = user_id;
    connection_auth_status_[connection_id] = true;
    
    std::cout << "User " << user_id << " bound to connection " << connection_id << std::endl;
}

uint32_t AuthCenter::getUserIdFromConnection(network::ConnectionId connection_id) {
    if (!initialized_ || !config_.enable_authentication) {
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = connection_user_map_.find(connection_id);
    if (it == connection_user_map_.end()) {
        return 0;
    }
    
    return it->second;
}

void AuthCenter::unbindConnection(network::ConnectionId connection_id) {
    if (!initialized_ || !config_.enable_authentication) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 从映射中移除
    auto user_it = connection_user_map_.find(connection_id);
    if (user_it != connection_user_map_.end()) {
        uint32_t user_id = user_it->second;
        user_connection_map_.erase(user_id);
        connection_user_map_.erase(user_it);
    }
    
    connection_auth_status_.erase(connection_id);
    
    std::cout << "Connection " << connection_id << " unbound from user" << std::endl;
}

bool AuthCenter::isConnectionAuthenticated(network::ConnectionId connection_id) {
    if (!initialized_ || !config_.enable_authentication) {
        return true;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = connection_auth_status_.find(connection_id);
    if (it == connection_auth_status_.end()) {
        return false;
    }
    
    return it->second;
}

void AuthCenter::setConnectionAuthenticated(network::ConnectionId connection_id, bool authenticated) {
    if (!initialized_ || !config_.enable_authentication) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    connection_auth_status_[connection_id] = authenticated;
}

// Base64编码（简化实现，用于JWT）
std::string base64Encode(const std::string& input) {
    static const std::string base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    
    std::string result;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    
    const unsigned char* bytes_to_encode = reinterpret_cast<const unsigned char*>(input.c_str());
    size_t in_len = input.size();
    
    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            
            for (i = 0; (i <4) ; i++)
                result += base64_chars[char_array_4[i]];
            i = 0;
        }
    }
    
    if (i) {
        for (j = i; j < 3; j++)
            char_array_3[j] = '\0';
            
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;
        
        for (j = 0; (j < i + 1); j++)
            result += base64_chars[char_array_4[j]];
            
        while ((i++ < 3))
            result += '=';
    }
    
    // 替换JWT不允许的字符
    std::replace(result.begin(), result.end(), '+', '-');
    std::replace(result.begin(), result.end(), '/', '_');
    result.erase(std::remove(result.begin(), result.end(), '='), result.end());
    
    return result;
}

// Base64解码（简化实现，用于JWT）
std::string base64Decode(const std::string& input) {
    static const std::string base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    
    std::string str = input;
    std::replace(str.begin(), str.end(), '-', '+');
    std::replace(str.begin(), str.end(), '_', '/');
    
    while (str.size() % 4 != 0) {
        str += '=';
    }
    
    std::string result;
    int i = 0;
    int j = 0;
    size_t in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    
    while (str.size() > in_ && str[in_] != '=') {
        char_array_4[i++] = str[in_]; in_++;
        if (i ==4) {
            for (i = 0; i <4; i++)
                char_array_4[i] = base64_chars.find(char_array_4[i]);
                
            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
            
            for (i = 0; (i < 3); i++)
                result += char_array_3[i];
            i = 0;
        }
    }
    
    if (i) {
        for (j = i; j <4; j++)
            char_array_4[j] = 0;
            
        for (j = 0; j <4; j++)
            char_array_4[j] = base64_chars.find(char_array_4[j]);
            
        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];
        
        for (j = 0; (j < i - 1); j++)
            result += char_array_3[j];
    }
    
    return result;
}

// 简化的HMAC-SHA256实现（仅用于演示，实际项目中应使用加密库）
std::string hmacSha256(const std::string& key, const std::string& data) {
    // 简化实现，实际项目中应使用openssl或其他加密库
    // 这里仅返回一个简单的哈希值作为演示
    std::string result;
    for (size_t i = 0; i < 32; ++i) {
        result += static_cast<char>((key[i % key.size()] + data[i % data.size()]) & 0xFF);
    }
    return result;
}

bool AuthCenter::parseJwtToken(const std::string& token, std::unordered_map<std::string, std::string>& payload) {
    // 解析JWT Token：header.payload.signature
    size_t first_dot = token.find('.');
    size_t second_dot = token.find('.', first_dot + 1);
    
    if (first_dot == std::string::npos || second_dot == std::string::npos) {
        return false;
    }
    
    // 提取payload
    std::string payload_str = base64Decode(token.substr(first_dot + 1, second_dot - first_dot - 1));
    
    // 简单JSON解析（仅支持一级键值对）
    size_t pos = 0;
    while (pos < payload_str.size()) {
        // 跳过空格和{}
        if (payload_str[pos] == '{' || payload_str[pos] == '}' || payload_str[pos] == ' ' || payload_str[pos] == '\t' || payload_str[pos] == '\n') {
            pos++;
            continue;
        }
        
        // 提取key
        size_t key_start = pos + 1; // 跳过""
        size_t key_end = payload_str.find('"', key_start);
        if (key_end == std::string::npos) {
            return false;
        }
        std::string key = payload_str.substr(key_start, key_end - key_start);
        pos = key_end + 2; // 跳过":"
        
        // 提取value
        size_t value_start = pos + (payload_str[pos] == '"' ? 1 : 0);
        size_t value_end = value_start;
        if (payload_str[value_start - 1] == '"') {
            // 字符串值
            value_end = payload_str.find('"', value_start);
        } else {
            // 数字值
            while (value_end < payload_str.size() && (isdigit(payload_str[value_end]) || payload_str[value_end] == '-')) {
                value_end++;
            }
        }
        
        if (value_end == std::string::npos) {
            return false;
        }
        
        std::string value = payload_str.substr(value_start, value_end - value_start);
        pos = value_end + (payload_str[value_end] == '"' ? 2 : 1); // 跳过"和,
        
        payload[key] = value;
    }
    
    return true;
}

std::string AuthCenter::generateJwtToken(const std::unordered_map<std::string, std::string>& payload) {
    // 构建header
    std::string header = R"({"alg":"HS256","typ":"JWT"})";
    
    // 构建payload JSON
    std::stringstream payload_json;
    payload_json << ")";
    bool first = true;
    for (const auto& pair : payload) {
        if (!first) {
            payload_json << ",";
        }
        first = false;
        payload_json << "\"" << pair.first << "\":" << "\"" << pair.second << "\"";
    }
    payload_json << ")";
    
    // 生成签名
    std::string encoded_header = base64Encode(header);
    std::string encoded_payload = base64Encode(payload_json.str());
    std::string signature_input = encoded_header + "." + encoded_payload;
    std::string signature = base64Encode(hmacSha256(config_.jwt_secret, signature_input));
    
    // 组合成完整Token
    return encoded_header + "." + encoded_payload + "." + signature;
}

} // namespace gateway