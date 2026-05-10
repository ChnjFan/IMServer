//
// Created by Fan on 2026/5/5.
//

#ifndef IMSERVER_ASIOIOSERVICEPOOL_H
#define IMSERVER_ASIOIOSERVICEPOOL_H

#include <vector>
#include <boost/asio.hpp>

#include "Singleton.h"

class AsioIOServicePool : public Singleton<AsioIOServicePool> {
public:
    // io_context 如果没有绑定socket，直接调用 run 会直接返回
    // 通过 work 绑定保证 io_context 没有事件也不会退出，防止事件循环因 “暂时无任务” 而提前退出
    using IOService = boost::asio::io_context;
    using Work = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;
    using WorkPtr = std::unique_ptr<Work>;

    ~AsioIOServicePool();
    AsioIOServicePool(const AsioIOServicePool&) = delete;
    AsioIOServicePool& operator=(const AsioIOServicePool&) = delete;

    // TODO:后续设置均衡策略，先用轮询
    IOService& getIOService();
    void stop();

private:
    friend class Singleton<AsioIOServicePool>;

    explicit AsioIOServicePool(std::size_t size = std::thread::hardware_concurrency() - 1);

    std::vector<IOService> ioServices_;
    std::vector<WorkPtr> works_;
    std::vector<std::thread> threads_;
    std::size_t nextIndex_;
};


#endif //IMSERVER_ASIOIOSERVICEPOOL_H