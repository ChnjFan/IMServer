#include <iostream>
#include <thread>
#include <chrono>

#include <boost/asio.hpp>

#include "Gateway.h"

#define GATEWAY_DEFAULT_THREAD_COUNT 4

int main() {
    try {
        auto thread_count = std::thread::hardware_concurrency();
        if (thread_count == 0) {
            thread_count = GATEWAY_DEFAULT_THREAD_COUNT;
        }
        boost::asio::io_context io_context(thread_count);

        // 创建工作守护者，防止IO上下文退出
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard = 
            boost::asio::make_work_guard(io_context);

        std::vector<std::thread> threads;
        for (int i = 0; i < thread_count; ++i) {
            threads.emplace_back([&io_context]() {
                io_context.run();
            });
        }

        gateway::Gateway gateway(io_context);
        // todo 配置文件读取
        gateway::GatewayConfig config;
        config.tcp_port = 8888;
        config.websocket_port = 9999;
        config.http_port = 8080;
        config.max_connections = 10000;
        config.idle_timeout = 300;
        config.auth_config.enable_authentication = true;
        config.auth_config.jwt_secret = "my_secret_key_for_gateway";
        config.auth_config.jwt_expire_time = 3600;
        config.enable_debug_log = true;
        
        gateway.initialize(config);
        gateway.start();

        std::cout << "Gateway started successfully!" << std::endl;
        std::cout << "Press Enter to stop..." << std::endl;
        
        // 等待用户输入
        std::cin.get();
        
        // 停止网关
        gateway.stop();
        
        // 停止IO上下文
        io_context.stop();
        
        // 等待所有线程结束
        for (auto& thread : threads) {
            thread.join();
        }
        
        std::cout << "Gateway stopped successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}