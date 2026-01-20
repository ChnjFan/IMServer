#include "Metrics.h"
#include <iostream>
#include <sstream>
#include <nlohmann/json.hpp>

namespace routing {

// 预定义指标名称初始化
const std::string Metrics::MESSAGE_COUNT = "message_count";
const std::string Metrics::MESSAGE_ERROR_COUNT = "message_error_count";
const std::string Metrics::MESSAGE_LATENCY = "message_latency";
const std::string Metrics::QUEUE_SIZE = "queue_size";
const std::string Metrics::ROUTE_COUNT = "route_count";
const std::string Metrics::ROUTE_ERROR_COUNT = "route_error_count";
const std::string Metrics::SERVICE_COUNT = "service_count";

Metrics::Metrics() : start_time_(std::chrono::steady_clock::now()) {
    // 初始化预定义指标
    counters_[MESSAGE_COUNT] = 0;
    counters_[MESSAGE_ERROR_COUNT] = 0;
    counters_[ROUTE_COUNT] = 0;
    counters_[ROUTE_ERROR_COUNT] = 0;
    counters_[SERVICE_COUNT] = 0;
    timers_[MESSAGE_LATENCY] = 0;
    timer_counters_[MESSAGE_LATENCY] = 0;
}

Metrics::~Metrics() {
}

void Metrics::incrementCounter(const std::string& name, int64_t value) {
    counters_[name] += value;
}

void Metrics::decrementCounter(const std::string& name, int64_t value) {
    counters_[name] -= value;
}

int64_t Metrics::getCounter(const std::string& name) const {
    auto it = counters_.find(name);
    if (it != counters_.end()) {
        return it->second;
    }
    return 0;
}

void Metrics::recordTimer(const std::string& name, int64_t duration) {
    timers_[name] += duration;
    timer_counters_[name]++;
}

void Metrics::recordTimer(const std::string& name, const std::chrono::steady_clock::time_point& start) {
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    recordTimer(name, duration);
}

double Metrics::getTimerAverage(const std::string& name) const {
    auto it = timers_.find(name);
    auto count_it = timer_counters_.find(name);
    if (it != timers_.end() && count_it != timer_counters_.end() && count_it->second > 0) {
        return static_cast<double>(it->second) / count_it->second;
    }
    return 0.0;
}

int64_t Metrics::getTimerTotal(const std::string& name) const {
    auto it = timers_.find(name);
    if (it != timers_.end()) {
        return it->second;
    }
    return 0;
}

int64_t Metrics::getUptime() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();
}

void Metrics::reset() {
    // 重置所有计数器
    for (auto& [name, counter] : counters_) {
        counter = 0;
    }
    // 重置所有计时器
    for (auto& [name, timer] : timers_) {
        timer = 0;
    }
    for (auto& [name, counter] : timer_counters_) {
        counter = 0;
    }
    // 重置启动时间
    start_time_ = std::chrono::steady_clock::now();
}

std::string Metrics::exportToJson() const {
    nlohmann::json j;
    
    // 导出计数器
    nlohmann::json counters_json;
    for (const auto& [name, value] : counters_) {
        counters_json[name] = value;
    }
    j["counters"] = counters_json;
    
    // 导出计时器
    nlohmann::json timers_json;
    for (const auto& [name, value] : timers_) {
        auto count_it = timer_counters_.find(name);
        if (count_it != timer_counters_.end()) {
            nlohmann::json timer_info;
            timer_info["total"] = value;
            timer_info["count"] = count_it->second;
            if (count_it->second > 0) {
                timer_info["average"] = static_cast<double>(value) / count_it->second;
            } else {
                timer_info["average"] = 0.0;
            }
            timers_json[name] = timer_info;
        }
    }
    j["timers"] = timers_json;
    
    // 导出运行时间
    j["uptime_seconds"] = getUptime();
    
    return j.dump(2);
}

std::string Metrics::exportToPrometheus() const {
    std::stringstream ss;
    
    // 导出计数器
    for (const auto& [name, value] : counters_) {
        ss << "im_routing_" << name << " " << value << "\n";
    }
    
    // 导出计时器
    for (const auto& [name, value] : timers_) {
        auto count_it = timer_counters_.find(name);
        if (count_it != timer_counters_.end()) {
            ss << "im_routing_" << name << "_total " << value << "\n";
            ss << "im_routing_" << name << "_count " << count_it->second << "\n";
            if (count_it->second > 0) {
                double average = static_cast<double>(value) / count_it->second;
                ss << "im_routing_" << name << "_average " << average << "\n";
            }
        }
    }
    
    // 导出运行时间
    ss << "im_routing_uptime_seconds " << getUptime() << "\n";
    
    return ss.str();
}

} // namespace routing
