#include "report.h"
#include <fstream>
#include <iostream>
#include <iomanip>

void ReportWriter::appendSample(time_t t, const Metrics& m) {
    samples_.push_back({t, m.toJson()});
}

void ReportWriter::writeSummary(const std::string& path) const {
    std::ofstream out(path);
    out << "=== Test Report ===\n";
    for (const auto& s : samples_) {
        out << "t=" << s.timestamp
            << " " << s.metrics.toStyledString();
    }
}

void ReportWriter::writeDataFile(const std::string& path) const {
    std::ofstream out(path);
    out << "timestamp,count,p50_us,p99_us,avg_us,errors,error_rate\n";
    for (const auto& s : samples_) {
        out << s.timestamp << ","
            << s.metrics["count"].asUInt64() << ","
            << s.metrics["p50_us"].asInt64() << ","
            << s.metrics["p99_us"].asInt64() << ","
            << s.metrics["avg_us"].asInt64() << ","
            << s.metrics["errors"].asUInt64() << ","
            << std::fixed << std::setprecision(4)
            << s.metrics["error_rate"].asDouble() << "\n";
    }
}
