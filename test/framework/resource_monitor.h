#ifndef IMSERVER_RESOURCE_MONITOR_H
#define IMSERVER_RESOURCE_MONITOR_H

#include <unistd.h>
#include <fstream>
#include <string>
#include <filesystem>

struct ResourceSnapshot {
    int   fdCount = 0;
    long  vmRSS_KB = 0;
    int   threadCount = 0;
};

class ResourceMonitor {
public:
    // 注意：/proc 文件系统仅存在于 Linux。macOS 上会捕获异常返回零值，
    // 此时 isLeaking 的 baseline > 0 条件不满足，自动跳过泄漏检测。
    ResourceSnapshot sample() const {
        ResourceSnapshot snap;
        try {
            // 统计 /proc/self/fd 数量（Linux）
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
        } catch (const std::exception&) {
            // 非 Linux 环境或 /proc 不可用：返回零值，isLeaking 自动跳过
        }
        return snap;
    }

    bool isLeaking(const ResourceSnapshot& baseline,
                   const ResourceSnapshot& current) const {
        // 描述符增长 >20% 或 RSS 增长 >30% 视为泄漏
        // baseline 为零（非 Linux 环境）时不会误报
        if (baseline.fdCount > 0 &&
            current.fdCount > baseline.fdCount * 1.2) return true;
        if (baseline.vmRSS_KB > 0 &&
            current.vmRSS_KB > baseline.vmRSS_KB * 1.3) return true;
        return false;
    }
};

#endif
