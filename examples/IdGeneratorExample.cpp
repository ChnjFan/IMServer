#include "../src/tool/IdGenerator.h"
#include "../src/network/TcpServer.h"
#include "../src/network/WebSocketServer.h"
#include "../src/network/HttpServer.h"
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

using namespace imserver;
using namespace imserver::tool;
using namespace imserver::network;

int main() {
    std::cout << "=== IdGenerator 使用示例 ===" << std::endl;
    
    // 1. 基本ID生成示例
    std::cout << "\n1. 基本ID生成示例:" << std::endl;
    
    auto& id_gen = IdGenerator::getInstance();
    
    // 生成各种类型的ID
    ConnectionId conn_id = id_gen.generateConnectionId();
    UserId user_id = id_gen.generateUserId();
    MessageId msg_id = id_gen.generateMessageId();
    SessionId session_id = id_gen.generateSessionId();
    
    std::cout << "连接ID: " << conn_id << std::endl;
    std::cout << "用户ID: " << user_id << std::endl;
    std::cout << "消息ID: " << msg_id << std::endl;
    std::cout << "会话ID: " << session_id << std::endl;
    
    // 2. 多线程ID生成测试
    std::cout << "\n2. 多线程ID生成测试:" << std::endl;
    
    std::vector<std::thread> threads;
    std::vector<ConnectionId> thread_ids;
    std::mutex ids_mutex;
    
    // 创建多个线程同时生成ID
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&id_gen, &thread_ids, &ids_mutex]() {
            for (int j = 0; j < 100; ++j) {
                auto id = id_gen.generateConnectionId();
                std::lock_guard<std::mutex> lock(ids_mutex);
                thread_ids.push_back(id);
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
    std::cout << "生成了 " << thread_ids.size() << " 个连接ID" << std::endl;
    std::cout << "ID范围: " << *std::min_element(thread_ids.begin(), thread_ids.end()) 
              << " - " << *std::max_element(thread_ids.begin(), thread_ids.end()) << std::endl;
    
    // 3. 其他ID类型示例
    std::cout << "\n3. 其他ID类型示例:" << std::endl;
    
    // 生成基于时间戳的ID
    TimestampId timestamp_id = id_gen.generateTimestampId("conn");
    std::cout << "基于时间戳的ID: " << timestamp_id << std::endl;
    
    // 生成UUID
    std::string uuid = id_gen.generateUuid();
    std::cout << "UUID: " << uuid << std::endl;
    
    // 生成短ID
    std::string short_id = id_gen.generateShortId(12);
    std::cout << "短ID: " << short_id << std::endl;
    
    // 4. 获取统计信息
    std::cout << "\n4. 生成器统计信息:" << std::endl;
    
    auto stats = id_gen.getStats();
    std::cout << "下一个连接ID: " << stats.next_connection_id << std::endl;
    std::cout << "下一个用户ID: " << stats.next_user_id << std::endl;
    std::cout << "下一个消息ID: " << stats.next_message_id << std::endl;
    std::cout << "下一个会话ID: " << stats.next_session_id << std::endl;
    std::cout << "总生成数量: " << stats.total_generated << std::endl;
    
    // 5. 在网络服务器中的使用示例
    std::cout << "\n5. 网络服务器中的使用示例:" << std::endl;
    
    try {
        // 创建IO上下文
        boost::asio::io_context io_context;
        
        // 创建连接管理器
        ConnectionManager connection_manager;
        
        // 创建TCP服务器，使用统一的ID生成
        auto tcp_server = std::make_shared<TcpServer>(
            io_context, connection_manager, "127.0.0.1", 8000);
        
        // 创建WebSocket服务器
        auto ws_server = std::make_shared<WebSocketServer>(
            io_context, "127.0.0.1", 8080);
        
        // 创建HTTP服务器
        auto http_server = std::make_shared<http::HttpServer>(io_context);
        
        std::cout << "所有服务器创建成功，都使用统一的ID生成器" << std::endl;
        std::cout << "TCP服务器端口: 8000" << std::endl;
        std::cout << "WebSocket服务器端口: 8080" << std::endl;
        std::cout << "HTTP服务器端口: 8081" << std::endl;
        
        // 注意：这里只是演示创建，不实际启动服务器
        // 在实际使用中，需要在单独的线程中运行io_context
        
    } catch (const std::exception& e) {
        std::cerr << "创建服务器时发生错误: " << e.what() << std::endl;
    }
    
    // 6. 性能测试
    std::cout << "\n6. ID生成性能测试:" << std::endl;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    const int test_count = 100000;
    
    for (int i = 0; i < test_count; ++i) {
        id_gen.generateConnectionId();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    std::cout << "生成 " << test_count << " 个连接ID耗时: " 
              << duration.count() / 1000.0 << " ms" << std::endl;
    std::cout << "平均每个ID耗时: " 
              << static_cast<double>(duration.count()) / test_count << " μs" << std::endl;
    std::cout << "每秒可生成ID数量: " 
              << static_cast<double>(test_count) * 1000000 / duration.count() << std::endl;
    
    std::cout << "\n=== 示例完成 ===" << std::endl;
    
    return 0;
}