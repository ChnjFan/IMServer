#include "metrics.h"
#include <sstream>
#include <iomanip>

void LatencyHistogram::record(int64_t us) {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        samples_.push_back(us);
        count_++;
    }

    min_ = std::min(min_, us);
    max_ = std::max(max_, us);
    sum_ += us;
}

int64_t LatencyHistogram::percentile(double p) const {
    if (samples_.empty()) return 0;
    auto sorted = samples_;
    std::sort(sorted.begin(), sorted.end());
    size_t idx = static_cast<size_t>(p * sorted.size());
    if (idx >= sorted.size()) idx = sorted.size() - 1;
    return sorted[idx];
}

int64_t LatencyHistogram::min() const { return samples_.empty() ? 0 : min_; }
int64_t LatencyHistogram::max() const { return samples_.empty() ? 0 : max_; }
int64_t LatencyHistogram::avg() const { return count_ ? sum_ / count_ : 0; }

void ThroughputCounter::tick() { total_++; }

void ErrorCounter::addFailed() { failed_++; }
void ErrorCounter::addSuccess() { success_++; }

double ErrorCounter::errorRate() const {
    uint64_t total = success_ + failed_;
    return total == 0 ? 0.0 : static_cast<double>(failed_) / total;
}

std::string Metrics::summary() const {
    std::ostringstream oss;
    oss << "count=" << latency.count()
        << " qps=" << std::fixed << std::setprecision(1)
        << (latency.count() /
            std::max(1.0, std::chrono::duration<double>(
                std::chrono::steady_clock::now() - startTime).count()))
        << " p50=" << latency.percentile(0.5) << "us"
        << " p99=" << latency.percentile(0.99) << "us"
        << " err=" << std::setprecision(3) << errors.errorRate() * 100 << "%";
    return oss.str();
}

Json::Value Metrics::toJson() const {
    Json::Value root;
    root["count"] = static_cast<Json::UInt64>(latency.count());
    root["p50_us"] = latency.percentile(0.5);
    root["p99_us"] = latency.percentile(0.99);
    root["avg_us"] = latency.avg();
    root["errors"] = static_cast<Json::UInt64>(errors.failed());
    root["error_rate"] = errors.errorRate();
    return root;
}
