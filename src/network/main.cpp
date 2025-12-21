#include "event_loop.h"
#include <iostream>
#include <chrono>

/**
 * @brief network模块的main函数
 * 
 * 注意：本IMServer采用多进程架构，每个模块作为单独进程运行
 * network模块作为独立进程，负责处理所有网络通信
 */
int main() {
    try {
        std::cout << "=== IMServer Network Module Starting ===" << std::endl;
        
        // 创建EventLoop实例，使用4个线程
        im::network::EventLoop event_loop(4);
        
        // 启动事件循环
        event_loop.start();
        
        std::cout << "Network Module EventLoop is running with " 
                  << event_loop.getThreadPoolSize() << " threads" << std::endl;
        
        // 模拟网络模块运行（实际应用中会有网络服务启动逻辑）
        std::cout << "Network Module is ready for handling network events..." << std::endl;
        
        // 保持进程运行（实际应用中会有更复杂的逻辑）
        while (event_loop.isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        std::cout << "=== IMServer Network Module Stopped ===" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Network Module Exception: " << e.what() << std::endl;
        return 1;
    }
}
