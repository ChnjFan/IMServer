#include "report_output.h"
#include <iostream>
#include <fstream>
#include <iomanip>

ReportOutput::ReportOutput(std::string /*name*/) {
    std::cout << "\n=== Stress Test Report ===" << std::endl;
}

ReportOutput::~ReportOutput() = default;

void ReportOutput::tick(const StressMetrics& m, int online, int tSeconds) {
    records_.push_back({
        tSeconds,
        online,
        m.msg_sent.load(),
        m.msg_recv.load(),
        m.chat_msg_sent.load(),
        m.chat_msg_recv.load(),
        m.friend_apply_sent.load(),
        m.friend_apply_recv.load(),
        m.user_search_sent.load(),
        m.user_search_recv.load(),
        m.rtt_hist.percentile(0.5),
        m.rtt_hist.percentile(0.99),
        m.errorRate()
    });

    std::cout << "[t=" << std::setw(4) << tSeconds << "s] "
              << "online=" << std::setw(5) << online
              << " msg/s=" << std::setw(5) << (records_.size() >= 2 ?
                  records_.back().msg_sent - records_[records_.size()-2].msg_sent : 0)
              << " rtt_p99=" << std::setw(7) << m.rtt_hist.percentile(0.99) << "us"
              << " err=" << std::fixed << std::setprecision(3) << m.errorRate() * 100 << "%"
              << std::endl;
}

void ReportOutput::summary(const StressMetrics& m, int target, int elapsedMs) {
    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Target:          " << target << std::endl;
    std::cout << "Connect success: " << m.connect_success.load() << std::endl;
    std::cout << "Handshake:       " << m.handshake_success.load() << std::endl;
    std::cout << "Peak online:     " << m.peak_online.load() << std::endl;
    std::cout << "RTT P50:         " << m.rtt_hist.percentile(0.5) << " us" << std::endl;
    std::cout << "RTT P99:         " << m.rtt_hist.percentile(0.99) << " us" << std::endl;
    std::cout << "RTT P999:        " << m.rtt_hist.percentile(0.999) << " us" << std::endl;
    std::cout << "Messages sent:   " << m.msg_sent.load() << std::endl;
    std::cout << "Messages recv:   " << m.msg_recv.load() << std::endl;
    std::cout << "  chat:          " << m.chat_msg_sent.load() << " sent / "
              << m.chat_msg_recv.load() << " recv" << std::endl;
    std::cout << "  friend_apply:  " << m.friend_apply_sent.load() << " sent / "
              << m.friend_apply_recv.load() << " recv" << std::endl;
    std::cout << "  user_search:   " << m.user_search_sent.load() << " sent / "
              << m.user_search_recv.load() << " recv" << std::endl;
    std::cout << "Disconnects:     " << m.disconnect_.load() << std::endl;
    std::cout << "Elapsed:         " << elapsedMs << " ms" << std::endl;
    std::cout << "==============================\n" << std::endl;
}

void ReportOutput::saveCsv(const std::string& path) {
    std::ofstream out(path);
    if (!out.is_open()) {
        std::cerr << "[ReportOutput] Failed to open " << path << std::endl;
        return;
    }

    out << "t,online,msg_sent,msg_recv,"
        << "chat_sent,chat_recv,friend_sent,friend_recv,query_sent,query_recv,"
        << "rtt_p50_us,rtt_p99_us,err_rate\n";
    for (const auto& r : records_) {
        out << r.t << ","
            << r.online << ","
            << r.msg_sent << ","
            << r.msg_recv << ","
            << r.chat_sent << ","
            << r.chat_recv << ","
            << r.friend_sent << ","
            << r.friend_recv << ","
            << r.query_sent << ","
            << r.query_recv << ","
            << r.rtt_p50 << ","
            << r.rtt_p99 << ","
            << std::fixed << std::setprecision(4) << r.err_rate << "\n";
    }

    std::cout << "[ReportOutput] CSV saved to " << path << std::endl;
}
