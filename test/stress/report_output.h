#ifndef IMSERVER_REPORT_OUTPUT_H
#define IMSERVER_REPORT_OUTPUT_H

#include "stress_metrics.h"
#include <string>
#include <vector>

class ReportOutput {
public:
    explicit ReportOutput(std::string name);
    ~ReportOutput();

    void tick(const StressMetrics& m, int online, int tSeconds);
    void summary(const StressMetrics& m, int target, int elapsedMs);
    void saveCsv(const std::string& path);

private:
    struct Record {
        int t;
        int online;
        uint64_t msg_sent;
        uint64_t msg_recv;
        uint64_t chat_sent;
        uint64_t chat_recv;
        uint64_t friend_sent;
        uint64_t friend_recv;
        uint64_t query_sent;
        uint64_t query_recv;
        int64_t rtt_p50;
        int64_t rtt_p99;
        double err_rate;
    };
    std::vector<Record> records_;
};

#endif // IMSERVER_REPORT_OUTPUT_H
