#include "IdGenerator.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <thread>
#include <array>
#include <algorithm>

// 跨平台进程ID获取支持
#ifdef _WIN32
    #include <windows.h>
    #include <process.h>
#elif defined(__linux__) || defined(__APPLE__)
    #include <unistd.h>
#endif

namespace imserver {
namespace tool {

// Base62字符集定义
const char* IdGenerator::BASE62_CHARS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

// ==================== 单例实现 ====================

IdGenerator& IdGenerator::getInstance() {
    static IdGenerator instance;
    return instance;
}

// ==================== 构造函数 ====================

IdGenerator::IdGenerator()
    : process_id_(getProcessId())
    , start_time_(std::chrono::steady_clock::now()) {
    // 初始化时记录启动信息
    total_generated_ = 0;
}

// ==================== 公共方法实现 ====================

IdGenerator::ConnectionId IdGenerator::generateConnectionId() {
    auto id = next_connection_id_++;
    total_generated_++;
    return id;
}

IdGenerator::UserId IdGenerator::generateUserId() {
    auto id = next_user_id_++;
    total_generated_++;
    return id;
}

IdGenerator::MessageId IdGenerator::generateMessageId() {
    auto id = next_message_id_++;
    total_generated_++;
    return id;
}

IdGenerator::SessionId IdGenerator::generateSessionId() {
    auto id = next_session_id_++;
    total_generated_++;
    return id;
}

IdGenerator::TimestampId IdGenerator::generateTimestampId() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - start_time_).count();
    
    // 结合时间戳、进程ID和计数器生成唯一ID
    TimestampId timestamp_part = static_cast<TimestampId>(duration) & 0xFFFFFFFFFFFF; // 取低48位
    TimestampId process_part = static_cast<TimestampId>(process_id_) & 0xFFFF; // 取低16位
    TimestampId counter_part = static_cast<TimestampId>(total_generated_.load()) & 0xFFFF; // 取低16位
    
    TimestampId result = (timestamp_part << 32) | (process_part << 16) | counter_part;
    
    total_generated_++;
    return result;
}

std::string IdGenerator::generateUuid() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 生成随机数
    std::random_device rd;
    std::mt19937_64 gen(rd() + std::hash<std::thread::id>{}(std::this_thread::get_id()));
    std::uniform_int_distribution<uint64_t> dis(0, UINT64_MAX);
    
    uint64_t time_low = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count());
    uint64_t time_mid = dis(gen);
    uint64_t time_hi_and_version = (4 << 12) | (dis(gen) & 0x0FFF); // 版本4
    uint64_t clock_seq_hi_and_reserved = (2 << 6) | (dis(gen) & 0x3F); // 变体10
    uint64_t clock_seq_low = dis(gen);
    uint64_t node = dis(gen);
    
    // 组合UUID字节
    std::array<uint8_t, 16> bytes;
    
    // 小端字节序
    for (int i = 0; i < 8; ++i) {
        bytes[i] = static_cast<uint8_t>((time_low >> (i * 8)) & 0xFF);
    }
    for (int i = 0; i < 2; ++i) {
        bytes[8 + i] = static_cast<uint8_t>((time_mid >> (i * 8)) & 0xFF);
    }
    bytes[10] = static_cast<uint8_t>((time_hi_and_version >> 8) & 0xFF);
    bytes[11] = static_cast<uint8_t>(time_hi_and_version & 0xFF);
    bytes[12] = static_cast<uint8_t>(clock_seq_hi_and_reserved & 0xFF);
    bytes[13] = static_cast<uint8_t>(clock_seq_low & 0xFF);
    for (int i = 0; i < 6; ++i) {
        bytes[14 + i] = static_cast<uint8_t>((node >> (i * 8)) & 0xFF);
    }
    
    // 格式化为UUID字符串
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    oss << std::setw(8) << (static_cast<uint32_t>(bytes[0]) << 24 | static_cast<uint32_t>(bytes[1]) << 16 | 
                           static_cast<uint32_t>(bytes[2]) << 8 | static_cast<uint32_t>(bytes[3])) << "-";
    oss << std::setw(4) << (static_cast<uint32_t>(bytes[4]) << 8 | static_cast<uint32_t>(bytes[5])) << "-";
    oss << std::setw(4) << (static_cast<uint32_t>(bytes[6]) << 8 | static_cast<uint32_t>(bytes[7])) << "-";
    oss << std::setw(2) << static_cast<uint32_t>(bytes[8]) << std::setw(2) << static_cast<uint32_t>(bytes[9]) << "-";
    oss << std::setw(2) << static_cast<uint32_t>(bytes[10]) << std::setw(2) << static_cast<uint32_t>(bytes[11]) << 
           std::setw(2) << static_cast<uint32_t>(bytes[12]) << std::setw(2) << static_cast<uint32_t>(bytes[13]) <<
           std::setw(2) << static_cast<uint32_t>(bytes[14]) << std::setw(2) << static_cast<uint32_t>(bytes[15]);
    
    total_generated_++;
    return oss.str();
}

std::string IdGenerator::generateShortId(size_t length) {
    if (length == 0 || length > 32) {
        length = 8; // 默认长度
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::random_device rd;
    std::mt19937_64 gen(rd() + std::hash<std::thread::id>{}(std::this_thread::get_id()));
    std::uniform_int_distribution<int> dis(0, 61); // Base62字符集索引0-61
    
    std::string result;
    result.reserve(length);
    
    for (size_t i = 0; i < length; ++i) {
        result += BASE62_CHARS[dis(gen)];
    }
    
    total_generated_++;
    return result;
}

void IdGenerator::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    next_connection_id_ = 1;
    next_user_id_ = 1;
    next_message_id_ = 1;
    next_session_id_ = 1;
    total_generated_ = 0;
}

IdGenerator::GeneratorStats IdGenerator::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    GeneratorStats stats;
    stats.next_connection_id = next_connection_id_.load();
    stats.next_user_id = next_user_id_.load();
    stats.next_message_id = next_message_id_.load();
    stats.next_session_id = next_session_id_.load();
    stats.total_generated = total_generated_.load();
    stats.start_time = start_time_;
    
    return stats;
}

// ==================== 私有方法实现 ====================

uint64_t IdGenerator::getCurrentTimestamp() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time_).count();
}

uint32_t IdGenerator::getProcessId() const {
#ifdef _WIN32
    // Windows下的进程ID获取
    return static_cast<uint32_t>(GetCurrentProcessId());
#elif defined(__linux__) || defined(__APPLE__)
    // Linux/macOS下的进程ID获取
    return static_cast<uint32_t>(getpid());
#else
    // 其他不支持的平台，抛出异常或返回默认值
    static_assert(false, "Unsupported operating system for process ID retrieval");
    return 0;
#endif
}

uint64_t IdGenerator::generateRandom(uint64_t min, uint64_t max) const {
    if (min > max) {
        std::swap(min, max);
    }
    
    std::random_device rd;
    std::mt19937_64 gen(rd() + std::hash<std::thread::id>{}(std::this_thread::get_id()));
    std::uniform_int_distribution<uint64_t> dis(min, max);
    
    return dis(gen);
}

} // namespace tool
} // namespace imserver