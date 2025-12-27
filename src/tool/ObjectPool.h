#pragma once

// 标准库头文件
#include <memory>
#include <queue>
#include <mutex>
#include <atomic>
#include <functional>
#include <stdexcept>
#include <chrono>
#include <vector>

namespace imserver {
namespace tool {

/**
 * @brief 通用对象池模板类
 * @tparam T 对象类型，必须支持默认构造函数
 * 
 * ObjectPool提供对象的复用机制，减少对象的动态分配和释放开销，
 * 提高系统性能，特别适用于频繁创建和销毁对象的场景。
 * 
 * @note 该类线程安全，支持多线程环境下并发访问
 */
template<typename T>
class ObjectPool {
public:
    /**
     * @brief 构造函数
     * @param initial_size 初始对象池大小
     * @param max_size 最大对象池大小
     * @param enable_auto_expand 是否启用自动扩容
     * @param expand_step 扩容步长
     * @param shrink_threshold 收缩阈值
     */
    explicit ObjectPool(
        size_t initial_size = 10,
        size_t max_size = 100,
        bool enable_auto_expand = true,
        size_t expand_step = 10,
        size_t shrink_threshold = 50);

    /**
     * @brief 析构函数
     */
    ~ObjectPool();

    /**
     * @brief 禁止拷贝和赋值
     */
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    /**
     * @brief 获取对象（如果没有可用对象则创建新的）
     * @param args 对象构造参数
     * @return 对象智能指针
     * 
     * @note 如果池中有可用对象，则复用对象；如果没有且未达到最大大小，则创建新对象
     */
    template<typename... Args>
    std::shared_ptr<T> acquire(Args&&... args);

    /**
     * @brief 归还对象到池中
     * @param object 要归还的对象智能指针
     * 
     * @note 归还的对象会被重置（调用reset方法）并放入池中
     */
    void release(std::shared_ptr<T> object);

    /**
     * @brief 预热对象池，创建指定数量的对象
     * @param count 要预热的对象数量
     * @param args 对象构造参数
     */
    template<typename... Args>
    void warmup(size_t count, Args&&... args);

    /**
     * @brief 清理池中所有对象
     * 
     * @note 清理后，池中的对象数量为0，但不会影响已经借出的对象
     */
    void clear();

    /**
     * @brief 强制收缩对象池
     * @param target_size 目标池大小
     * 
     * @note 收缩后池中的可用对象数量不会超过target_size
     */
    void shrink(size_t target_size);

    /**
     * @brief 扩容对象池
     * @param count 扩容数量
     * @param args 对象构造参数
     */
    template<typename... Args>
    void expand(size_t count, Args&&... args);

    /**
     * @brief 获取池统计信息
     */
    struct PoolStats {
        size_t total_objects = 0;      // 总对象数
        size_t available_objects = 0;  // 可用对象数
        size_t acquired_objects = 0;   // 已借出对象数
        size_t max_size = 0;           // 最大对象数
        size_t total_acquisitions = 0; // 总获取次数
        size_t total_releases = 0;     // 总归还次数
        size_t failed_acquisitions = 0; // 获取失败次数
        std::chrono::steady_clock::time_point last_activity_time; // 最后活动时间
    };

    /**
     * @brief 获取池统计信息
     * @return 统计信息
     */
    PoolStats getStats() const;

    /**
     * @brief 设置对象重置回调函数
     * @param reset_func 重置回调函数
     * 
     * @note 在对象归还到池中时会调用此回调函数，用于重置对象状态
     */
    void setResetCallback(std::function<void(T&)> reset_func);

    /**
     * @brief 设置对象构造回调函数
     * @param construct_func 构造回调函数
     * 
     * @note 在创建新对象时会调用此回调函数，用于自定义对象初始化
     */
    void setConstructCallback(std::function<void(T&)> construct_func);

    /**
     * @brief 检查池是否需要扩容
     * @return 是否需要扩容
     */
    bool needsExpansion() const;

    /**
     * @brief 检查池是否需要收缩
     * @return 是否需要收缩
     */
    bool needsShrinkage() const;

private:
    /**
     * @brief 创建新对象
     * @param args 对象构造参数
     * @return 新对象智能指针
     */
    template<typename... Args>
    std::shared_ptr<T> createObject(Args&&... args);

    /**
     * @brief 自动调整池大小
     */
    void autoAdjust();

    /**
     * @brief 记录活动
     */
    void recordActivity();

private:
    // 可用对象队列
    std::queue<std::shared_ptr<T>> available_objects_;
    
    // 池配置
    size_t max_size_;
    bool enable_auto_expand_;
    size_t expand_step_;
    size_t shrink_threshold_;
    
    // 统计信息
    std::atomic<size_t> total_objects_{0};
    std::atomic<size_t> total_acquisitions_{0};
    std::atomic<size_t> total_releases_{0};
    std::atomic<size_t> failed_acquisitions_{0};
    std::chrono::steady_clock::time_point last_activity_time_;
    
    // 线程安全保护
    mutable std::mutex pool_mutex_;
    
    // 回调函数
    std::function<void(T&)> reset_callback_;
    std::function<void(T&)> construct_callback_;
};

// 预定义别名
using ConnectionPool = ObjectPool<void>;

} // namespace tool
} // namespace imserver

// 模板实现
#include "ObjectPool.tpp"