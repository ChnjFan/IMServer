#include <iostream>

#include "GateServer.h"

int main()
{
    try {
        auto port = static_cast<unsigned short>(8080);
        net::io_context io_context{1};
        // 捕获信号结束进程
        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&io_context](const boost::system::error_code &error, int signal_number) {
            if (error) {
                return;
            }
            io_context.stop();
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