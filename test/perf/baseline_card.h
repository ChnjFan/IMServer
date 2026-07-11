#ifndef IMSERVER_BASELINE_CARD_H
#define IMSERVER_BASELINE_CARD_H

#include "perf_suite.h"
#include <sstream>
#include <iomanip>
#include <string>

// 将 PerfSuite 阶梯施压的结果格式化为文本基线卡
inline std::string formatBaselineCard(const std::vector<PerfLevel>& levels,
                                      const std::string& date,
                                      const std::string& machine) {
    std::ostringstream oss;
    oss << "┌────────────────────────────────────┐\n";
    oss << "│  Performance Baseline — ChatServer │\n";
    oss << "├────────────────────────────────────┤\n";
    oss << "│  日期      " << std::setw(24) << std::left << date << "│\n";
    oss << "│  机器      " << std::setw(24) << machine << "│\n";
    oss << "│  │                                 │\n";
    oss << "│  并发     QPS      P50    P99      │\n";

    for (auto& lvl : levels) {
        oss << "│  " << std::setw(7) << lvl.clientCount
            << "  " << std::setw(7) << std::fixed << std::setprecision(0) << lvl.qps
            << "  " << std::setw(5) << (lvl.p50_us / 1000) << "ms"
            << "  " << std::setw(5) << (lvl.p99_us / 1000) << "ms   │\n";
    }

    oss << "└────────────────────────────────────┘\n";
    return oss.str();
}

#endif
