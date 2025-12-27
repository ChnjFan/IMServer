#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace imserver {
namespace tool {

/**
 * @brief 全局唯一ID生成器
 * 
 * 提供线程安全的全局唯一ID生成功能，支持多种ID类型：
 * - 连接ID (ConnectionId): 用于标识网络连接
 * - 用户ID (UserId): 用于标识用户
 * - 消息ID (MessageId): 用于标识消息
 * - 会话ID (SessionId): 用于标识会话
 * 
 * @note 该类为单例模式，线程安全，支持高并发场景
 */
class IdGenerator {
public:
    using ConnectionId = uint64_t;
    using UserId = uint64_t;
    using MessageId = uint64_t;
    using SessionId = uint64_t;
    using TimestampId = uint64_t;

    /**
     * @brief 获取单例实例
     * @return IdGenerator实例引用
     */
    static IdGenerator& getInstance();

    /**
     * @brief 禁止拷贝和赋值
     */
    IdGenerator(const IdGenerator&) = delete;
    IdGenerator& operator=(const IdGenerator&) = delete;

    /**
     * @brief 生成全局唯一连接ID
     * @return 连接ID (ConnectionId)
     * 
     * @note 该方法生成的ID在整个进程生命周期内唯一
     */
    ConnectionId generateConnectionId();

    /**
     * @brief 生成全局唯一用户ID
     * @return 用户ID (UserId)
     * 
     * @note 该方法生成的ID在整个进程生命周期内唯一
     */
    UserId generateUserId();

    /**
     * @brief 生成全局唯一消息ID
     * @return 消息ID (MessageId)
     * 
     * @note 该方法生成的ID在整个进程生命周期内唯一
     */
    MessageId generateMessageId();

    /**
     * @brief 生成全局唯一会话ID
     * @return 会话ID (SessionId)
     * 
     * @note 该方法生成的ID在整个进程生命周期内唯一
     */
    SessionId generateSessionId();

    /**
     * @brief 生成基于时间戳的唯一ID
     * @return 基于时间戳的ID (TimestampId)
     * 
     * @note 该方法结合时间戳和计数器生成ID，适合分布式场景
     */
    TimestampId generateTimestampId();

    /**
     * @brief 生成UUID格式的字符串ID
     * @return UUID字符串
     * 
     * @note 该方法生成标准UUID格式的字符串，用于需要字符串ID的场景
     */
    std::string generateUuid();

    /**
     * @brief 生成短ID（Base62编码）
     * @param length ID长度，默认8位
     * @return Base62编码的短ID字符串
     * 
     * @note 该方法生成适合URL等场景的短ID
     */
    std::string generateShortId(size_t length = 8);

    /**
     * @brief 重置所有计数器（主要用于测试）
     */
    void reset();

    /**
     * @brief 获取生成器统计信息
     */
    struct GeneratorStats {
        ConnectionId next_connection_id = 1;
        UserId next_user_id = 1;
        MessageId next_message_id = 1;
        SessionId next_session_id = 1;
        uint64_t total_generated = 0;
        std::chrono::steady_clock::time_point start_time;
    };

    /**
     * @brief 获取统计信息
     * @return 统计信息
     */
    GeneratorStats getStats() const;

private:
    /**
     * @brief 私有构造函数（单例模式）
     */
    IdGenerator();

    /**
     * @brief 获取当前时间戳（毫秒）
     * @return 当前时间戳
     */
    uint64_t getCurrentTimestamp() const;

    /**
     * @brief 获取进程ID
     * @return 进程ID
     */
    uint32_t getProcessId() const;

    /**
     * @brief 生成随机数
     * @param min 最小值
     * @param max 最大值
     * @return 随机数
     */
    uint64_t generateRandom(uint64_t min, uint64_t max) const;

private:
    // 原子计数器
    std::atomic<ConnectionId> next_connection_id_{1};
    std::atomic<UserId> next_user_id_{1};
    std::atomic<MessageId> next_message_id_{1};
    std::atomic<SessionId> next_session_id_{1};
    std::atomic<uint64_t> total_generated_{0};

    // 进程信息
    const uint32_t process_id_;
    const std::chrono::steady_clock::time_point start_time_;

    // 线程安全
    mutable std::mutex mutex_;

    // Base62字符集
    static const char* BASE62_CHARS;
};

/**
 * @brief 连接ID类型别名，用于网络层
 */
using ConnectionId = IdGenerator::ConnectionId;

} // namespace tool
} // namespace imserver