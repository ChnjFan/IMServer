#include "../src/network/TcpServer.h"
#include "../src/network/ConnectionManager.h"
#include "../src/tool/IdGenerator.h"
#include <iostream>
#include <boost/asio.hpp>
#include <memory>
#include <thread>
#include <chrono>

using namespace imserver;
using namespace imserver::network;
using namespace imserver::tool;

int main() {
    std::cout << "=== TcpServer + IdGenerator 测试 ===" << std::endl;
    
    try {
        // 创建IO上下文
        boost::asio::io_context io_context;
        
        // 创建连接管理器
        ConnectionManager connection_manager;
        
        // 创建TCP服务器
        TcpServer tcp_server(io_context, connection_manager, "127.0.0.1", 8080);
        
        std::cout << "TCP服务器创建成功，监听地址: 127.0.0.1:8080" << std::endl;
        
        // 启动服务器
        tcp_server.start();
        
        // 测试IdGenerator功能
        auto& id_gen = IdGenerator::getInstance();
        std::cout << "测试ID生成器:" << std::endl;
        std::cout << "- 连接ID: " << id_gen.generateConnectionId() << std::endl;
        std::cout << "- 用户ID: " << id_gen.generateUserId() << std::endl;
        std::cout << "- 消息ID: " << id_gen.generateMessageId() << std::endl;
        std::cout << "- 会话ID: " << id_gen.generateSessionId() << std::endl;
        std::cout << "- UUID: " << id_gen.generateUuid() << std::endl;
        std::cout << "- 短ID: " << id_gen.generateShortId() << std::endl;
        
        std::cout << "\n服务器已启动，正在等待连接..." << std::endl;
        std::cout << "请使用以下命令测试连接:" << std::endl;
        std::cout << "  telnet 127.0.0.1 8080" << std::endl;
        std::cout << "按Ctrl+C停止服务器" << std::endl;
        
        // 在一个单独线程中运行IO上下文
        std::thread io_thread([&io_context]() {
            io_context.run();
        });
        
        // 主线程等待
        std::cout << "\n按Enter键停止服务器..." << std::endl;
        std::cin.get();
        
        // 停止服务器
        std::cout << "正在停止服务器..." << std::endl;
        tcp_server.stop();
        
        // 停止IO上下文
        io_context.stop();
        
        // 等待线程结束
        if (io_thread.joinable()) {
            io_thread.join();
        }
        
        std::cout << "服务器已成功停止" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}