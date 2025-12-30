#include "../src/network/ConnectionManager.h"
#include "../src/network/HttpServer.h"
#include "../src/tool/IdGenerator.h"
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

using namespace imserver;
using namespace imserver::network;
using namespace imserver::network::http;
using namespace imserver::tool;

int main() {
    std::cout << "=== HttpServer with ConnectionManager 示例 ===" << std::endl;
    
    try {
        // 创建IO上下文
        boost::asio::io_context io_context;
        
        // 创建连接管理器
        ConnectionManager connection_manager;
        
        // 初始化连接管理器的清理定时器
        connection_manager.initializeCleanupTimer(io_context);
        
        // 创建HTTP服务器，使用ConnectionManager
        HttpServer http_server(io_context, connection_manager);
        
        // 注册路由
        http_server.get("/", [](const HttpRequest& request, HttpResponse& response) {
            response.result(http::status::ok);
            response.set(http::field::content_type, "text/html");
            response.body() = "<h1>Welcome to IMServer HTTP Server</h1>";
        });
        
        http_server.get("/status", [&connection_manager](const HttpRequest& request, HttpResponse& response) {
            auto stats = connection_manager.getGlobalStats();
            
            response.result(http::status::ok);
            response.set(http::field::content_type, "application/json");
            response.body() = "{";
            response.body() += "\"total_connections\": " + std::to_string(stats.total_connections) + ",";
            response.body() += "\"active_connections\": " + std::to_string(stats.active_connections) + ",";
            response.body() += "\"http_connections\": " + std::to_string(stats.http_connections) + ",";
            response.body() += "\"tcp_connections\": " + std::to_string(stats.tcp_connections) + ",";
            response.body() += "\"websocket_connections\": " + std::to_string(stats.websocket_connections) + "";
            response.body() += "}";
        });
        
        // 启动HTTP服务器
        http_server.start(boost::asio::ip::address::from_string("127.0.0.1"), 8080);
        
        std::cout << "HTTP服务器已启动，监听端口 8080" << std::endl;
        std::cout << "访问地址: http://127.0.0.1:8080" << std::endl;
        std::cout << "状态地址: http://127.0.0.1:8080/status" << std::endl;
        
        // 启动IO上下文
        std::thread io_thread([&io_context]() {
            io_context.run();
        });
        
        // 运行一段时间
        std::cout << "\n服务器将运行 30 秒..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(30));
        
        // 停止服务器
        http_server.stop();
        io_context.stop();
        
        if (io_thread.joinable()) {
            io_thread.join();
        }
        
        std::cout << "\n=== HTTP服务器已停止 ===" << std::endl;
        
        // 输出最终统计信息
        auto stats = connection_manager.getGlobalStats();
        std::cout << "\n最终统计信息:" << std::endl;
        std::cout << "总连接数: " << stats.total_connections << std::endl;
        std::cout << "活跃连接数: " << stats.active_connections << std::endl;
        std::cout << "HTTP连接数: " << stats.http_connections << std::endl;
        std::cout << "TCP连接数: " << stats.tcp_connections << std::endl;
        std::cout << "WebSocket连接数: " << stats.websocket_connections << std::endl;
        std::cout << "总发送字节数: " << stats.total_bytes_sent << std::endl;
        std::cout << "总接收字节数: " << stats.total_bytes_received << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}