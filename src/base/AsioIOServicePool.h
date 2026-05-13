#ifndef IMSERVER_ASIOIOSERVICEPOOL_H
#define IMSERVER_ASIOIOSERVICEPOOL_H

#include <vector>
#include <boost/asio.hpp>
#include <atomic>

#include "Singleton.h"

class AsioIOServicePool : public Singleton<AsioIOServicePool> {
public:
    using IOService = boost::asio::io_context;
    using Work = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;
    using WorkPtr = std::unique_ptr<Work>;

    ~AsioIOServicePool();
    AsioIOServicePool(const AsioIOServicePool&) = delete;
    AsioIOServicePool& operator=(const AsioIOServicePool&) = delete;

    IOService& getIOService();
    void stop();

private:
    friend class Singleton<AsioIOServicePool>;

    explicit AsioIOServicePool(std::size_t size = std::thread::hardware_concurrency() - 1);

    std::vector<IOService> ioServices_;
    std::vector<WorkPtr> works_;
    std::vector<std::thread> threads_;
    std::size_t nextIndex_;
    std::atomic<bool> stopped_{false};
};

#endif //IMSERVER_ASIOIOSERVICEPOOL_H
