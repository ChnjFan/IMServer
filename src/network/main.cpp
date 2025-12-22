#include "EventSystem.h"
#include "TcpServer.h"
#include <iostream>
#include <chrono>
#include <string>

/**
 * @brief network模块的main函数
 * 
 * 注意：本IMServer采用多进程架构，每个模块作为单独进程运行
 * network模块作为独立进程，负责处理所有网络通信
 */
int main() {
    try {
        std::cout << "=== IMServer Network Module Starting ===" << std::endl;
        
        // 获取EventSystem单例实例
        auto& event_system = im::network::EventSystem::getInstance();
        
        // 启动EventSystem
        event_system.start();
        std::cout << "EventSystem is running" << std::endl;
        
        // 创建TCP服务器
        im::network::TcpServer tcp_server(event_system, "0.0.0.0", 8000);
        
        // 设置消息处理回调
        tcp_server.setMessageHandler([](im::network::Connection::Ptr conn, const std::vector<char>& data) {
            // 处理接收到的消息
            std::string message(data.begin(), data.end());
            auto remote_endpoint = conn->getRemoteEndpoint();
            std::cout << "[TCP] Received from " << remote_endpoint.address().to_string() 
                      << ":" << remote_endpoint.port() << ": " << message << std::endl;
            
            // 回显消息
            std::string response = "Echo: " + message;
            conn->send(std::vector<char>(response.begin(), response.end()));
        });
        
        // 设置连接关闭回调
        tcp_server.setCloseHandler([](im::network::Connection::Ptr conn) {
            auto remote_endpoint = conn->getRemoteEndpoint();
            std::cout << "[TCP] Connection closed from " << remote_endpoint.address().to_string() 
                      << ":" << remote_endpoint.port() << std::endl;
        });
        
        // 启动TCP服务器
        tcp_server.start();
        std::cout << "TCP Server is listening on port 8000" << std::endl;
        
        // 保持进程运行
        std::cout << "Network Module is ready for handling network events..." << std::endl;
        
        // 等待用户输入退出命令
        std::cout << "Press Enter to stop the network module..." << std::endl;
        std::cin.get();
        
        // 停止TCP服务器
        tcp_server.stop();
        
        // 停止EventSystem
        event_system.stop();
        
        std::cout << "=== IMServer Network Module Stopped ===" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Network Module Exception: " << e.what() << std::endl;
        return 1;
    }
}
