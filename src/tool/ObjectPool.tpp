#pragma once

#include "ObjectPool.h"

namespace imserver {
namespace tool {

template<typename T>
ObjectPool<T>::ObjectPool(
    size_t initial_size,
    size_t max_size,
    bool enable_auto_expand,
    size_t expand_step,
    size_t shrink_threshold)
    : max_size_(max_size)
    , enable_auto_expand_(enable_auto_expand)
    , expand_step_(expand_step)
    , shrink_threshold_(shrink_threshold) {
    
    if (initial_size > max_size_) {
        initial_size = max_size_;
    }
    
    // 预创建初始对象
    if (initial_size > 0) {
        warmup(initial_size);
    }
}

template<typename T>
ObjectPool<T>::~ObjectPool() {
    clear();
}

template<typename T>
template<typename... Args>
std::shared_ptr<T> ObjectPool<T>::acquire(Args&&... args) {
    std::unique_lock<std::mutex> lock(pool_mutex_);
    
    recordActivity();
    total_acquisitions_++;
    
    // 如果池中有可用对象，直接复用
    if (!available_objects_.empty()) {
        auto object = available_objects_.front();
        available_objects_.pop();
        
        // 记录获取成功
        return object;
    }
    
    // 池中没有可用对象，尝试扩容
    if (total_objects_.load() < max_size_) {
        lock.unlock();
        return expand(1, std::forward<Args>(args)...);
    }
    
    // 已达到最大对象数，获取失败
    failed_acquisitions_++;
    
    // 如果启用自动扩容，尝试扩容（这里应该已经检查过，但在自动扩容模式下可能需要重新检查）
    if (enable_auto_expand_ && total_objects_.load() < max_size_) {
        lock.unlock();
        return expand(1, std::forward<Args>(args)...);
    }
    
    // 无法创建新对象，抛出异常
    throw std::runtime_error("ObjectPool is full and cannot acquire more objects");
}

template<typename T>
void ObjectPool<T>::release(std::shared_ptr<T> object) {
    if (!object) {
        return;
    }
    
    std::unique_lock<std::mutex> lock(pool_mutex_);
    
    recordActivity();
    total_releases_++;
    
    // 调用重置回调函数
    if (reset_callback_) {
        try {
            reset_callback_(*object);
        } catch (const std::exception& e) {
            // 重置失败，记录错误但不影响归还流程
            // 在实际应用中可以考虑记录日志
        }
    }
    
    // 检查是否超过最大对象数
    if (available_objects_.size() < max_size_) {
        available_objects_.push(object);
    } else {
        // 超过最大对象数，不放入池中，对象将自动销毁
        // 这种情况下，记录一个事件以供监控
    }
    
    // 如果启用自动调整，进行调整
    if (enable_auto_expand_) {
        autoAdjust();
    }
}

template<typename T>
template<typename... Args>
void ObjectPool<T>::warmup(size_t count, Args&&... args) {
    std::unique_lock<std::mutex> lock(pool_mutex_);
    
    for (size_t i = 0; i < count && total_objects_.load() < max_size_; ++i) {
        try {
            auto object = createObject(std::forward<Args>(args)...);
            available_objects_.push(object);
        } catch (const std::exception& e) {
            // 创建失败，跳过继续创建其他对象
            break;
        }
    }
}

template<typename T>
void ObjectPool<T>::clear() {
    std::unique_lock<std::mutex> lock(pool_mutex_);
    
    // 清空可用对象队列
    while (!available_objects_.empty()) {
        available_objects_.pop();
    }
    
    // 重置统计信息
    total_objects_.store(0);
    total_acquisitions_.store(0);
    total_releases_.store(0);
    failed_acquisitions_.store(0);
    last_activity_time_ = std::chrono::steady_clock::now();
}

template<typename T>
void ObjectPool<T>::shrink(size_t target_size) {
    std::unique_lock<std::mutex> lock(pool_mutex_);
    
    target_size = std::min(target_size, max_size_);
    
    while (available_objects_.size() > target_size && !available_objects_.empty()) {
        available_objects_.pop();
        total_objects_--;
    }
}

template<typename T>
template<typename... Args>
std::shared_ptr<T> ObjectPool<T>::expand(size_t count, Args&&... args) {
    std::unique_lock<std::mutex> lock(pool_mutex_);
    
    size_t objects_to_create = std::min(count, max_size_ - total_objects_.load());
    
    for (size_t i = 0; i < objects_to_create; ++i) {
        try {
            auto object = createObject(std::forward<Args>(args)...);
            available_objects_.push(object);
        } catch (const std::exception& e) {
            // 创建失败，停止创建
            break;
        }
    }
    
    lock.unlock();
    
    // 如果有可用对象，直接获取一个
    if (!available_objects_.empty()) {
        std::unique_lock<std::mutex> inner_lock(pool_mutex_);
        auto object = available_objects_.front();
        available_objects_.pop();
        return object;
    }
    
    // 扩容后仍然没有可用对象，抛出异常
    throw std::runtime_error("Failed to create objects during expansion");
}

template<typename T>
typename ObjectPool<T>::PoolStats ObjectPool<T>::getStats() const {
    std::unique_lock<std::mutex> lock(pool_mutex_);
    
    PoolStats stats;
    stats.total_objects = total_objects_.load();
    stats.available_objects = available_objects_.size();
    stats.acquired_objects = stats.total_objects - stats.available_objects;
    stats.max_size = max_size_;
    stats.total_acquisitions = total_acquisitions_.load();
    stats.total_releases = total_releases_.load();
    stats.failed_acquisitions = failed_acquisitions_.load();
    stats.last_activity_time = last_activity_time_;
    
    return stats;
}

template<typename T>
void ObjectPool<T>::setResetCallback(std::function<void(T&)> reset_func) {
    std::unique_lock<std::mutex> lock(pool_mutex_);
    reset_callback_ = std::move(reset_func);
}

template<typename T>
void ObjectPool<T>::setConstructCallback(std::function<void(T&)> construct_func) {
    std::unique_lock<std::mutex> lock(pool_mutex_);
    construct_callback_ = std::move(construct_func);
}

template<typename T>
bool ObjectPool<T>::needsExpansion() const {
    std::unique_lock<std::mutex> lock(pool_mutex_);
    
    // 如果可用对象数少于初始大小的10%，需要扩容
    return available_objects_.size() < expand_step_ / 10;
}

template<typename T>
bool ObjectPool<T>::needsShrinkage() const {
    std::unique_lock<std::mutex> lock(pool_mutex_);
    
    // 如果可用对象数超过阈值，需要收缩
    return available_objects_.size() > shrink_threshold_;
}

template<typename T>
template<typename... Args>
std::shared_ptr<T> ObjectPool<T>::createObject(Args&&... args) {
    // 创建新对象
    auto object = std::make_shared<T>(std::forward<Args>(args)...);
    
    total_objects_++;
    last_activity_time_ = std::chrono::steady_clock::now();
    
    // 调用构造回调函数
    if (construct_callback_) {
        try {
            construct_callback_(*object);
        } catch (const std::exception& e) {
            // 构造回调失败，记录错误但不影响对象创建
        }
    }
    
    return object;
}

template<typename T>
void ObjectPool<T>::autoAdjust() {
    // 检查是否需要扩容
    if (needsExpansion() && total_objects_.load() < max_size_) {
        expand(expand_step_);
    }
    
    // 检查是否需要收缩
    if (needsShrinkage()) {
        shrink(max_size_ / 2); // 收缩到最大容量的一半
    }
}

template<typename T>
void ObjectPool<T>::recordActivity() {
    last_activity_time_ = std::chrono::steady_clock::now();
}

} // namespace tool
} // namespace imserver