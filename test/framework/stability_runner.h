#ifndef IMSERVER_STABILITY_RUNNER_H
#define IMSERVER_STABILITY_RUNNER_H

#include "metrics.h"
#include "resource_monitor.h"
#include "report.h"
#include <functional>
#include <string>
#include <atomic>

struct StabilityConfig {
    std::string chatHost;
    uint16_t chatPort;
    int clientCount = 10;
    int durationSec = 3600;
    std::chrono::seconds heartbeatInterval{30};
    std::chrono::seconds onlineSec{60};
    std::chrono::seconds offlineSec{10};
    int msgPerClientPerSec = 1;
    std::string reportPath = "stability_report.csv";
};

class StabilityRunner {
public:
    explicit StabilityRunner(StabilityConfig config);

    // 运行单个场景，返回是否满足通过标准
    bool runKeepAlive();
    bool runChurn();
    bool runMessageStorm();
    bool runMixed();

    const Metrics& metrics() const { return metrics_; }
    const ReportWriter& report() const { return report_; }

private:
    bool runScenario(void (*fn)(int, const StabilityConfig&, Metrics&, std::atomic<bool>&));

    StabilityConfig config_;
    Metrics metrics_;
    ReportWriter report_;
    ResourceMonitor resourceMon_;
};

#endif
