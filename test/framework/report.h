#ifndef IMSERVER_REPORT_H
#define IMSERVER_REPORT_H

#include "metrics.h"
#include <string>
#include <vector>

struct SampleRecord {
    time_t timestamp;
    Json::Value metrics;
};

class ReportWriter {
public:
    void appendSample(time_t t, const Metrics& m);
    void writeSummary(const std::string& path) const;
    void writeDataFile(const std::string& path) const;

private:
    std::vector<SampleRecord> samples_;
};

#endif
