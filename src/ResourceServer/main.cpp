//
// Created by Fan on 2026/7/2.
//

#include <iostream>

#include "AsioIOServicePool.h"
#include "ConfigMgr.h"
#include "net/ResourceServer.h"

int main() {
    auto port = std::stoi(ConfigMgr::getInstance()["ResourceServer"]["Port"]);

    try {
        net::io_context io_context{1};
        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        auto pool = AsioIOServicePool::getInstance();
        signals.async_wait([&io_context, &pool](const boost::system::error_code &error, int signal_number) {
            if (error) {
                return;
            }
            io_context.stop();
            pool->stop();
        });
        std::make_shared<ResourceServer>(io_context, port)->start();
        std::cout << "ResourceServer started on HTTP port: " << port << std::endl;
        io_context.run();
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return 0;
}
