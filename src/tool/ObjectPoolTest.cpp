#include "ObjectPool.h"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>

using namespace imserver::tool;

// 测试用的简单类
class TestObject {
public:
    TestObject() : id_(0), value_(0) {
        std::cout << "TestObject default constructor" << std::endl;
    }
    
    TestObject(int id, std::string value) : id_(id), value_(std::move(value)) {
        std::cout << "TestObject parameterized constructor: id=" << id_ << ", value=" << value_ << std::endl;
    }
    
    void reset() {
        id_ = 0;
        value_.clear();
        std::cout << "TestObject reset" << std::endl;
    }
    
    int getId() const { return id_; }
    std::string getValue() const { return value_; }
    
private:
    int id_;
    std::string value_;
};

// 测试基本功能
void testBasicFunctionality() {
    std::cout << "\n=== 测试基本功能 ===" << std::endl;
    
    ObjectPool<TestObject> pool(2, 10, true, 5, 8);
    
    // 测试预热
    std::cout << "预热对象池..." << std::endl;
    pool.warmup(3, 100, "preheated");
    
    auto stats = pool.getStats();
    std::cout << "池统计信息: 总对象=" << stats.total_objects 
              << ", 可用=" << stats.available_objects 
              << ", 已借出=" << stats.acquired_objects << std::endl;
    
    // 测试获取对象
    std::cout << "\n获取对象..." << std::endl;
    auto obj1 = pool.acquire(1, "first");
    auto obj2 = pool.acquire(2, "second");
    auto obj3 = pool.acquire(3, "third");
    
    std::cout << "obj1: id=" << obj1->getId() << ", value=" << obj1->getValue() << std::endl;
    std::cout << "obj2: id=" << obj2->getId() << ", value=" << obj2->getValue() << std::endl;
    std::cout << "obj3: id=" << obj3->getId() << ", value=" << obj3->getValue() << std::endl;
    
    // 测试归还对象
    std::cout << "\n归还对象..." << std::endl;
    pool.release(obj1);
    pool.release(obj2);
    pool.release(obj3);
    
    stats = pool.getStats();
    std::cout << "归还后统计: 总对象=" << stats.total_objects 
              << ", 可用=" << stats.available_objects 
              << ", 已借出=" << stats.acquired_objects << std::endl;
    
    // 测试复用
    std::cout << "\n测试对象复用..." << std::endl;
    auto reused = pool.acquire();
    std::cout << "复用对象: id=" << reused->getId() << ", value=" << reused->getValue() << std::endl;
    
    pool.release(reused);
}

// 测试并发功能
void testConcurrentAccess() {
    std::cout << "\n=== 测试并发访问 ===" << std::endl;
    
    ObjectPool<TestObject> pool(5, 50, true, 10, 40);
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};
    
    auto worker = [&pool, &success_count, &failure_count](int thread_id, int iterations) {
        for (int i = 0; i < iterations; ++i) {
            try {
                auto obj = pool.acquire(thread_id * 1000 + i, "thread_" + std::to_string(thread_id));
                
                // 模拟一些工作
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                
                pool.release(obj);
                success_count++;
            } catch (const std::exception& e) {
                failure_count++;
                std::cout << "线程 " << thread_id << " 失败: " << e.what() << std::endl;
            }
        }
    };
    
    // 创建多个线程
    std::vector<std::thread> threads;
    int num_threads = 5;
    int iterations_per_thread = 20;
    
    auto start_time = std::chrono::steady_clock::now();
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker, i, iterations_per_thread);
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    auto stats = pool.getStats();
    std::cout << "并发测试完成:" << std::endl;
    std::cout << "  成功: " << success_count << std::endl;
    std::cout << "  失败: " << failure_count << std::endl;
    std::cout << "  耗时: " << duration.count() << "ms" << std::endl;
    std::cout << "  最终池统计: 总对象=" << stats.total_objects 
              << ", 可用=" << stats.available_objects 
              << ", 已借出=" << stats.acquired_objects << std::endl;
}

// 测试回调函数
void testCallbacks() {
    std::cout << "\n=== 测试回调函数 ===" << std::endl;
    
    ObjectPool<TestObject> pool(2, 10);
    
    // 设置重置回调
    pool.setResetCallback([](TestObject& obj) {
        std::cout << "重置回调: 将对象重置为默认值" << std::endl;
        obj.reset();
    });
    
    // 设置构造回调
    pool.setConstructCallback([](TestObject& obj) {
        std::cout << "构造回调: 初始化新创建的对象" << std::endl;
        // 这里可以对新对象进行额外的初始化
    });
    
    std::cout << "获取并修改对象..." << std::endl;
    auto obj = pool.acquire(999, "callback_test");
    std::cout << "对象状态: id=" << obj->getId() << ", value=" << obj->getValue() << std::endl;
    
    std::cout << "归还对象（将触发重置回调）..." << std::endl;
    pool.release(obj);
    
    std::cout << "复用对象（将触发构造回调）..." << std::endl;
    auto reused = pool.acquire();
    std::cout << "复用对象状态: id=" << reused->getId() << ", value=" << reused->getValue() << std::endl;
    
    pool.release(reused);
}

// 测试异常处理
void testExceptionHandling() {
    std::cout << "\n=== 测试异常处理 ===" << std::endl;
    
    ObjectPool<TestObject> pool(0, 2); // 最小池，最大2个对象
    
    try {
        std::cout << "尝试获取3个对象（超过最大值）..." << std::endl;
        auto obj1 = pool.acquire(1, "first");
        auto obj2 = pool.acquire(2, "second");
        std::cout << "成功获取2个对象" << std::endl;
        
        // 第三个应该失败
        auto obj3 = pool.acquire(3, "third");
        std::cout << "意外：成功获取第三个对象" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "捕获异常（预期的）: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "ObjectPool 功能测试开始" << std::endl;
    
    try {
        testBasicFunctionality();
        testConcurrentAccess();
        testCallbacks();
        testExceptionHandling();
        
        std::cout << "\n所有测试完成！" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "测试失败: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}