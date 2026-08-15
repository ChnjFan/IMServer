#include "stress_connection_pool.h"
#include "account_manager.h"

#include <iostream>
#include <random>

StressConnectionPool::StressConnectionPool(int ioContextNum) {
    // 初始化 io_context 池
    for (int i = 0; i < ioContextNum; ++i) {
        auto worker = IoContextWorker{
            std::make_unique<net::io_context>(),
            nullptr,
            {}
        };
        worker.work = std::make_unique<WorkGuard>(worker.io->get_executor());
        workers_.push_back(std::move(worker));
    }

    // 启动 io 线程
    for (auto& w : workers_) {
        w.thread = std::thread([this, &w] { ioThreadFunc(*w.io); });
    }

    // 启动调度器
    schedulerIo_ = std::make_unique<net::io_context>();
    schedulerWork_ = std::make_unique<WorkGuard>(schedulerIo_->get_executor());
    schedulerThread_ = std::thread([this] {
        schedulerIo_->run();
    });
}

StressConnectionPool::~StressConnectionPool() {
    gracefulShutdown();
}

void StressConnectionPool::ioThreadFunc(net::io_context& io) {
    while (true) {
        try {
            io.run();
            break;  // io_context 退出
        } catch (const std::exception& e) {
            std::cerr << "[StressPool] io_context error: " << e.what() << std::endl;
        }
    }
}

void StressConnectionPool::addAndConnect(const std::vector<TestAccount>& accounts,
                                         int batchSize,
                                         std::chrono::milliseconds batchInterval) {
    // 创建客户端 (不立即连接)
    std::vector<StressTestClient::Ptr> newClients;
    {
        std::lock_guard<std::mutex> lock(clientsMtx_);
        for (const auto& acct : accounts) {
            int idx = nextIoIndex_.fetch_add(1) % workers_.size();
            auto client = std::make_shared<StressTestClient>(*workers_[idx].io, &metrics_);
            client->setLoginInfo(acct.uid, acct.token);
            clients_.push_back(client);
            newClients.push_back(client);
        }
    }

    // 分批调度连接 - 每个 batch 移动自己的 shared_ptr 子集到 lambda 中
    size_t sent = 0;
    while (sent < newClients.size()) {
        size_t end = std::min(sent + static_cast<size_t>(batchSize), newClients.size());

        // 按值捕获当前批次的 shared_ptr，避免悬空引用
        std::vector<StressTestClient::Ptr> batchClients(
            newClients.begin() + sent, newClients.begin() + end);

        // 同时拷贝当前批次的 account 信息
        std::vector<TestAccount> batchAccounts(
            accounts.begin() + sent, accounts.begin() + end);

        net::post(*schedulerIo_, [this, batchClients, batchAccounts] {
            for (size_t i = 0; i < batchClients.size(); ++i) {
                tcp::endpoint ep(
                    net::ip::make_address(batchAccounts[i].host),
                    static_cast<uint16_t>(batchAccounts[i].port)
                );
                batchClients[i]->asyncConnect(ep);
            }
        });

        sent = end;

        // 等待批次间隔
        if (sent < newClients.size()) {
            std::this_thread::sleep_for(batchInterval);
        }
    }
}

int StressConnectionPool::disconnectRandom(int count) {
    auto online = getOnlineClients();
    if (online.empty()) return 0;

    // 随机打乱
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(online.begin(), online.end(), gen);

    int actual = 0;
    for (size_t i = 0; i < std::min<size_t>(count, online.size()); ++i) {
        online[i]->close();
        actual++;
    }
    return actual;
}

std::vector<StressTestClient::Ptr> StressConnectionPool::getOnlineClients() const {
    std::lock_guard<std::mutex> lock(clientsMtx_);
    std::vector<StressTestClient::Ptr> result;
    for (const auto& c : clients_) {
        if (c->state() == ClientState::ONLINE) {
            result.push_back(c);
        }
    }
    return result;
}

std::vector<StressTestClient::Ptr> StressConnectionPool::sampleOnlineClients(size_t count) const {
    auto online = getOnlineClients();
    if (online.size() <= count) return online;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(online.begin(), online.end(), gen);
    online.resize(count);
    return online;
}

void StressConnectionPool::gracefulShutdown() {
    // 停止调度器
    if (schedulerWork_) {
        schedulerWork_.reset();
    }
    if (schedulerIo_) {
        schedulerIo_->stop();
    }
    if (schedulerThread_.joinable()) {
        schedulerThread_.join();
    }

    // 关闭所有客户端
    {
        std::lock_guard<std::mutex> lock(clientsMtx_);
        for (auto& c : clients_) {
            c->close();
        }
        clients_.clear();
    }

    // 停止 io_context
    for (auto& w : workers_) {
        if (w.work) {
            w.work.reset();
        }
        if (w.io) {
            w.io->stop();
        }
    }

    // 等待线程退出
    for (auto& w : workers_) {
        if (w.thread.joinable()) {
            w.thread.join();
        }
    }
}
