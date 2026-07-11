#ifndef IMSERVER_METRICS_H
#define IMSERVER_METRICS_H

#include <cstdint>
#include <chrono>
#include <vector>
#include <algorithm>
#include <numeric>
#include <string>
#include <json/json.h>

class LatencyHistogram {
public:
    void record(int64_t us);
    int64_t percentile(double p) const;
    int64_t min() const;
    int64_t max() const;
    int64_t avg() const;
    uint64_t count() const { return count_; }

private:
    std::vector<int64_t> samples_;
    uint64_t count_ = 0;
    int64_t min_ = INT64_MAX;
    int64_t max_ = 0;
    int64_t sum_ = 0;
};

class ThroughputCounter {
public:
    void tick();
    uint64_t total() const { return total_; }
private:
    std::atomic<uint64_t> total_{0};
};

class ErrorCounter {
public:
    void addFailed();
    void addSuccess();
    uint64_t failed() const { return failed_; }
    uint64_t success() const { return success_; }
    double errorRate() const;

private:
    std::atomic<uint64_t> failed_{0};
    std::atomic<uint64_t> success_{0};
};

struct Metrics {
    LatencyHistogram latency;
    ThroughputCounter throughput;
    ErrorCounter errors;
    std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();

    std::string summary() const;
    Json::Value toJson() const;
};

#endif
