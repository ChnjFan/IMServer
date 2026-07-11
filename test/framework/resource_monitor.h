#ifndef IMSERVER_RESOURCE_MONITOR_H
#define IMSERVER_RESOURCE_MONITOR_H

#include <unistd.h>
#include <fstream>
#include <string>

struct ResourceSnapshot {
    int   fdCount = 0;
    long  vmRSS_KB = 0;
    int   threadCount = 0;
};

class ResourceMonitor {
public:
    ResourceSnapshot sample() const {
        ResourceSnapshot snap;
        // 统计 /proc/self/fd 数量（Linux）
        snap.fdCount = 0;
        for (auto& p : std::filesystem::directory_iterator("/proc/self/fd")) {
            (void)p;
            snap.fdCount++;
        }
        // 读取 VmRSS（Linux /proc/self/status）
        std::ifstream status("/proc/self/status");
        std::string line;
        while (std::getline(status, line)) {
            if (line.find("VmRSS:") == 0) {
                snap.vmRSS_KB = std::stol(line.substr(7));
                break;
            }
        }
        return snap;
    }

    bool isLeaking(const ResourceSnapshot& baseline,
                   const ResourceSnapshot& current) const {
        // 描述符增长 >20% 或 RSS 增长 >30% 视为泄漏
        if (baseline.fdCount > 0 &&
            current.fdCount > baseline.fdCount * 1.2) return true;
        if (baseline.vmRSS_KB > 0 &&
            current.vmRSS_KB > baseline.vmRSS_KB * 1.3) return true;
        return false;
    }
};

#endif
