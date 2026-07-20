#ifndef IMSERVER_PERF_SUITE_H
#define IMSERVER_PERF_SUITE_H

#include "metrics.h"
#include <string>
#include <vector>

// 一个负载台阶的测量结果
struct PerfLevel {
    int clientCount;
    double qps;
    int64_t p50_us;
    int64_t p99_us;
    double errorRate;
};

class PerfSuite {
public:
    struct Config {
        std::string chatHost;
        uint16_t chatPort;
        int stepSec = 60;        // 每个台阶的稳态时长（秒）
        std::string reportPath = "perf_report.csv";
    };

    explicit PerfSuite(Config config);

    // 阶梯施压：10 → 50 → 100 → 200 → 500 → 1000 → 5000 → 10000，记录每级稳态 QPS/延迟
    std::vector<PerfLevel> runRampUp();

    // 极限施压：clientCount ×2 直到错误率 > 5%，返回崩溃点
    int runToBreak();

    // 读写混合：70% 消息 + 20% 心跳 + 10% 上下线，固定 60s
    PerfLevel runMixedWorkload(int clientCount = 100);

    const Metrics& metrics() const { return metrics_; }

private:
    Config config_;
    Metrics metrics_;
};

#endif
