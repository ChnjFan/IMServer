#ifndef IMSERVER_STRESS_CONNECTION_POOL_H
#define IMSERVER_STRESS_CONNECTION_POOL_H

#include <memory>
#include <thread>
#include <vector>

#include <boost/asio.hpp>

#include "stress_test_client.h"
#include "stress_metrics.h"

namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

struct TestAccount;

/**
 * @brief 压力测试连接池
 *
 * 管理多个 StressTestClient，使用多 io_context 轮询分配连接。
 * 每个 io_context 跑在一个独立线程上。
 * 分批 connect 调度器避免 SYN 风暴。
 */
class StressConnectionPool {
public:
    /**
     * @param ioContextNum io_context 数量 (建议 = hw_concurrency - 2)
     */
    explicit StressConnectionPool(int ioContextNum);
    ~StressConnectionPool();

    // 禁止拷贝
    StressConnectionPool(const StressConnectionPool&) = delete;
    StressConnectionPool& operator=(const StressConnectionPool&) = delete;

    // === 公开接口 ===

    /**
     * @brief 添加一批账号并异步连接
     * @param accounts 测试账号列表
     * @param batchSize 每批连接数
     * @param batchInterval 批次间隔
     */
    void addAndConnect(const std::vector<TestAccount>& accounts,
                       int batchSize = 100,
                       std::chrono::milliseconds batchInterval = std::chrono::milliseconds(200));

    /**
     * @brief 断开指定数量连接
     * @param count 断开数量
     * @return 实际断开数量
     */
    int disconnectRandom(int count);

    /**
     * @brief 获取在线客户端
     */
    std::vector<StressTestClient::Ptr> getOnlineClients() const;

    /**
     * @brief 获取随机子集的在线客户端
     */
    std::vector<StressTestClient::Ptr> sampleOnlineClients(size_t count) const;

    /**
     * @brief 优雅关闭所有连接
     */
    void gracefulShutdown();

    // === 指标 ===
    StressMetrics& metrics() { return metrics_; }
    const StressMetrics& metrics() const { return metrics_; }

    // === 状态 ===
    int onlineCount() const { return static_cast<int>(metrics_.current_online.load()); }
    int totalClients() const { return static_cast<int>(clients_.size()); }

private:
    // io_context work guard 类型 (Boost 1.90)
    using WorkGuard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

    struct IoContextWorker {
        std::unique_ptr<net::io_context> io;
        std::unique_ptr<WorkGuard> work;
        std::thread thread;
    };

    void ioThreadFunc(net::io_context& io);

    // io_context 池
    std::vector<IoContextWorker> workers_;
    std::atomic<int> nextIoIndex_{0};

    // 调度器 (用于分批 connect)
    std::unique_ptr<net::io_context> schedulerIo_;
    std::unique_ptr<WorkGuard> schedulerWork_;
    std::thread schedulerThread_;

    // 所有客户端
    std::vector<StressTestClient::Ptr> clients_;
    mutable std::mutex clientsMtx_;

    // 指标
    StressMetrics metrics_;
};

#endif // IMSERVER_STRESS_CONNECTION_POOL_H
