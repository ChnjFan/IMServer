#include <iostream>

#include "AsioIOServicePool.h"
#include "ConfigMgr.h"
#include "GateServer.h"

int main()
{
    auto port = std::stoi(ConfigMgr::getInstance()["GateServer"]["Port"]);

    try {
        net::io_context io_context{1};
        // 捕获信号结束进程
        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        auto pool = AsioIOServicePool::getInstance();
        signals.async_wait([&io_context, &pool](const boost::system::error_code &error, int signal_number) {
            if (error) {
                return;
            }
            io_context.stop();
            pool->stop();
        });
        std::make_shared<GateServer>(io_context, port)->start();
        std::cout << "GateServer started on port: " << port << std::endl;
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return 0;
}
