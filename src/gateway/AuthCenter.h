#pragma once

#include <string>
#include <mutex>
#include <memory>
#include <atomic>
#include <unordered_map>

namespace gateway {

struct AuthConfig {
    bool enable_authentication = true;
    std::string jwt_secret = "default_secret_key";
    uint32_t jwt_expire_time = 3600; // 1小时
};

/**
 * @brief 认证中心类
 * 
 * 负责处理网关层的认证和授权逻辑，包括：
 * - 用户认证
 * - Token管理
 * - 权限验证
 * - 会话管理
 */
class AuthCenter {
public:
    AuthCenter();
    
    AuthCenter(const AuthCenter&) = delete;
    AuthCenter& operator=(const AuthCenter&) = delete;
    
    /**
     * @brief 初始化认证中心
     * @param config 网关配置
     */
    void initialize(const AuthConfig& config);
    
    /**
     * @brief 验证Token
     * @param token Token字符串
     * @return bool Token是否有效
     */
    bool validateToken(const std::string& token);
    
    /**
     * @brief 生成Token
     * @param user_id 用户ID
     * @param username 用户名
     * @return std::string 生成的Token
     */
    std::string generateToken(uint32_t user_id, const std::string& username);
    
    /**
     * @brief 从Token中提取用户ID
     * @param token Token字符串
     * @return uint32_t 用户ID
     */
    uint32_t getUserIdFromToken(const std::string& token);
    
    /**
     * @brief 从Token中提取用户名
     * @param token Token字符串
     * @return std::string 用户名
     */
    std::string getUsernameFromToken(const std::string& token);
    
    /**
     * @brief 刷新Token
     * @param old_token 旧Token
     * @return std::string 新Token
     */
    std::string refreshToken(const std::string& old_token);
    
    /**
     * @brief 使Token失效
     * @param token Token字符串
     */
    void invalidateToken(const std::string& token);
    
    /**
     * @brief 检查用户是否具有某个权限
     * @param user_id 用户ID
     * @param permission 权限标识
     * @return bool 是否具有权限
     */
    bool checkPermission(uint32_t user_id, const std::string& permission);
    
    /**
     * @brief 为用户添加权限
     * @param user_id 用户ID
     * @param permission 权限标识
     */
    void addPermission(uint32_t user_id, const std::string& permission);
    
    /**
     * @brief 从用户移除权限
     * @param user_id 用户ID
     * @param permission 权限标识
     */
    void removePermission(uint32_t user_id, const std::string& permission);
    
    /**
     * @brief 绑定用户ID到连接ID
     * @param user_id 用户ID
     * @param connection_id 连接ID
     */
    void bindUserIdToConnection(uint32_t user_id, network::ConnectionId connection_id);
    
    /**
     * @brief 从连接ID获取用户ID
     * @param connection_id 连接ID
     * @return uint32_t 用户ID
     */
    uint32_t getUserIdFromConnection(network::ConnectionId connection_id);
    
    /**
     * @brief 解绑用户ID和连接ID
     * @param connection_id 连接ID
     */
    void unbindConnection(network::ConnectionId connection_id);
    
    /**
     * @brief 检查连接是否已认证
     * @param connection_id 连接ID
     * @return bool 是否已认证
     */
    bool isConnectionAuthenticated(network::ConnectionId connection_id);
    
    /**
     * @brief 设置连接认证状态
     * @param connection_id 连接ID
     * @param authenticated 是否已认证
     */
    void setConnectionAuthenticated(network::ConnectionId connection_id, bool authenticated);
    
private:
    /**
     * @brief 解析JWT Token
     * @param token Token字符串
     * @param payload 输出的payload
     * @return bool 是否解析成功
     */
    bool parseJwtToken(const std::string& token, std::unordered_map<std::string, std::string>& payload);
    
    /**
     * @brief 生成JWT Token
     * @param payload JWT负载
     * @return std::string 生成的Token
     */
    std::string generateJwtToken(const std::unordered_map<std::string, std::string>& payload);
    
    // 配置
    AuthConfig config_;
    
    // 状态
    std::atomic<bool> initialized_{false};
    
    // 互斥锁
    std::mutex mutex_;
    
    // 用户ID到连接ID的映射
    std::unordered_map<uint32_t, network::ConnectionId> user_connection_map_;
    
    // 连接ID到用户ID的映射
    std::unordered_map<network::ConnectionId, uint32_t> connection_user_map_;
    
    // 连接认证状态映射
    std::unordered_map<network::ConnectionId, bool> connection_auth_status_;
    
    // 用户权限映射
    std::unordered_map<uint32_t, std::unordered_map<std::string, bool>> user_permissions_;
    
    // 失效Token列表（黑名单）
    std::unordered_map<std::string, bool> invalid_tokens_;
};

} // namespace gateway