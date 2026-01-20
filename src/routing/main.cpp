#include <iostream>
#include <signal.h>
#include <thread>
#include <chrono>

#include "RoutingService.h"

// 全局路由服务实例
routing::RoutingService* g_routing_service = nullptr;

/**
 * @brief 信号处理函数
 * @param sig 信号编号
 */
void signalHandler(int sig) {
    std::cout << "Received signal " << sig << ", shutting down..." << std::endl;
    
    if (g_routing_service) {
        g_routing_service->stop();
    }
    
    exit(0);
}

/**
 * @brief 主函数
 * @param argc 参数个数
 * @param argv 参数列表
 * @return 退出码
 */
int main(int argc, char** argv) {
    try {
        int port = 50050;
        if (argc > 1) {
            port = std::stoi(argv[1]);
        }
        
        std::cout << "Starting Routing Service on port " << port << "..." << std::endl;
        
        g_routing_service = new routing::RoutingService(port);
        
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);
        
        if (!g_routing_service->start()) {
            std::cerr << "Failed to start routing service" << std::endl;
            delete g_routing_service;
            return 1;
        }
        
        std::cout << "Routing service started successfully!" << std::endl;
        std::cout << "Press Ctrl+C to stop the service" << std::endl;
        
        // 主循环
        while (true) {
            // 定期检查服务状态
            if (!g_routing_service->isRunning()) {
                std::cerr << "Routing service stopped unexpectedly" << std::endl;
                break;
            }
            
            // 睡眠一段时间
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        delete g_routing_service;
        g_routing_service = nullptr;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        
        if (g_routing_service) {
            delete g_routing_service;
            g_routing_service = nullptr;
        }
        
        return 1;
    }
}
