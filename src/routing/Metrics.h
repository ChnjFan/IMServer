#pragma once

#include <unordered_map>
#include <atomic>
#include <string>
#include <chrono>
#include <memory>

namespace routing {

/**
 * @brief 监控指标类
 * 负责收集和管理路由服务的监控指标
 */
class Metrics {
private:
    // 计数器指标
    std::unordered_map<std::string, std::atomic<int64_t>> counters_;
    // 计时器指标（毫秒）
    std::unordered_map<std::string, std::atomic<int64_t>> timers_;
    // 计时器计数
    std::unordered_map<std::string, std::atomic<int64_t>> timer_counters_;
    // 启动时间
    std::chrono::steady_clock::time_point start_time_;

public:
    Metrics();
    ~Metrics();

    /**
     * @brief 增加计数器
     * @param name 计数器名称
     * @param value 增加值
     */
    void incrementCounter(const std::string& name, int64_t value = 1);

    /**
     * @brief 减少计数器
     * @param name 计数器名称
     * @param value 减少值
     */
    void decrementCounter(const std::string& name, int64_t value = 1);

    /**
     * @brief 获取计数器值
     * @param name 计数器名称
     * @return 计数器值
     */
    int64_t getCounter(const std::string& name) const;

    /**
     * @brief 记录计时器
     * @param name 计时器名称
     * @param duration 持续时间（毫秒）
     */
    void recordTimer(const std::string& name, int64_t duration);

    /**
     * @brief 记录计时器（自动计算）
     * @param name 计时器名称
     * @param start 开始时间
     */
    void recordTimer(const std::string& name, const std::chrono::steady_clock::time_point& start);

    /**
     * @brief 获取计时器平均值
     * @param name 计时器名称
     * @return 平均值（毫秒）
     */
    double getTimerAverage(const std::string& name) const;

    /**
     * @brief 获取计时器总时间
     * @param name 计时器名称
     * @return 总时间（毫秒）
     */
    int64_t getTimerTotal(const std::string& name) const;

    /**
     * @brief 获取服务运行时间
     * @return 运行时间（秒）
     */
    int64_t getUptime() const;

    /**
     * @brief 重置所有指标
     */
    void reset();

    /**
     * @brief 导出指标为JSON
     * @return JSON字符串
     */
    std::string exportToJson() const;

    /**
     * @brief 导出指标为Prometheus格式
     * @return Prometheus格式字符串
     */
    std::string exportToPrometheus() const;

    // 预定义的指标名称
    static const std::string MESSAGE_COUNT;          // 消息处理总数
    static const std::string MESSAGE_ERROR_COUNT;    // 消息处理失败数
    static const std::string MESSAGE_LATENCY;        // 消息处理延迟
    static const std::string QUEUE_SIZE;             // 队列大小
    static const std::string ROUTE_COUNT;            // 路由成功数
    static const std::string ROUTE_ERROR_COUNT;      // 路由失败数
    static const std::string SERVICE_COUNT;          // 服务实例数
};

} // namespace routing
