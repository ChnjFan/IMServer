#include <iostream>
#include <exception>

#include "ChatServer.h"
#include "ConfigMgr.h"
#include "AsioIOServicePool.h"

int main()
{
    const auto portStr = ConfigMgr::getInstance()["ChatServer"]["Port"];
    if (portStr.empty()) {
        std::cout << "Chat server port config is empty!" << std::endl;
        return 0;
    }
    auto port = std::stoi(portStr);

    try {
        net::io_context io_context{1};
        // 捕获信号结束进程
        auto pool = AsioIOServicePool::getInstance();
        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&io_context, &pool](const boost::system::error_code &error, int signal_number) {
            if (error) {
                return;
            }
            boost::ignore_unused(signal_number);
            io_context.stop();
            pool->stop();
        });
        std::make_shared<ChatServer>(io_context, port);
        std::cout << "ChatServer started on port: " << port << std::endl;
        io_context.run();
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return 0;
}