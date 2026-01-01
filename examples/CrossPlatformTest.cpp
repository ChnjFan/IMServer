#include "../src/tool/IdGenerator.h"
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>

using namespace imserver::tool;

int main() {
    std::cout << "=== IdGenerator 跨平台测试 ===" << std::endl;
    
    auto& id_gen = IdGenerator::getInstance();
    
    // 1. 测试进程ID获取
    std::cout << "\n1. 测试进程ID获取:" << std::endl;
    uint32_t process_id = id_gen.getProcessId();
    std::cout << "当前进程ID: " << process_id << std::endl;
    
    // 2. 测试基本ID生成
    std::cout << "\n2. 测试基本ID生成:" << std::endl;
    auto conn_id = id_gen.generateConnectionId();
    auto user_id = id_gen.generateUserId();
    auto msg_id = id_gen.generateMessageId();
    
    std::cout << "生成的连接ID: " << conn_id << std::endl;
    std::cout << "生成的用户ID: " << user_id << std::endl;
    std::cout << "生成的消息ID: " << msg_id << std::endl;
    
    // 3. 测试短ID生成
    std::cout << "\n3. 测试短ID生成:" << std::endl;
    std::string short_id = id_gen.generateShortId(8);
    std::cout << "生成的8位短ID: " << short_id << std::endl;
    
    // 4. 测试UUID生成
    std::cout << "\n4. 测试UUID生成:" << std::endl;
    std::string uuid = id_gen.generateUuid();
    std::cout << "生成的UUID: " << uuid << std::endl;
    
    // 5. 测试时间戳ID
    std::cout << "\n5. 测试时间戳ID:" << std::endl;
    auto timestamp_id = id_gen.generateTimestampId("test");
    std::cout << "生成的时间戳ID: " << timestamp_id << std::endl;
    
    // 6. 性能测试
    std::cout << "\n6. 性能测试:" << std::endl;
    const int test_count = 10000;
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < test_count; ++i) {
        id_gen.generateConnectionId();
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "生成 " << test_count << " 个连接ID耗时: " 
              << duration.count() / 1000.0 << " ms" << std::endl;
    
    // 7. 输出统计信息
    std::cout << "\n7. 统计信息:" << std::endl;
    auto stats = id_gen.getStats();
    std::cout << "总生成数量: " << stats.total_generated << std::endl;
    std::cout << "下一个连接ID: " << stats.next_connection_id << std::endl;
    
    // 8. 写入测试报告
    std::ofstream report("test_report.txt");
    if (report.is_open()) {
        report << "IdGenerator 跨平台测试报告\n";
        report << "===========================\n";
        report << "测试时间: " << std::chrono::system_clock::now().time_since_epoch().count() << "\n";
        report << "进程ID: " << process_id << "\n";
        report << "连接ID: " << conn_id << "\n";
        report << "用户ID: " << user_id << "\n";
        report << "消息ID: " << msg_id << "\n";
        report << "短ID: " << short_id << "\n";
        report << "UUID: " << uuid << "\n";
        report << "时间戳ID: " << timestamp_id << "\n";
        report << "性能测试 (" << test_count << " IDs): " 
               << duration.count() / 1000.0 << " ms\n";
        report << "总生成数量: " << stats.total_generated << "\n";
        report.close();
        std::cout << "\n测试报告已保存到 test_report.txt" << std::endl;
    }
    
    std::cout << "\n=== 跨平台测试完成 ===" << std::endl;
    
    return 0;
}