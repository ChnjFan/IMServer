/**
 * @file EventSystem.h
 * @brief 事件系统类，用于管理事件和事件监听器
 * 
 * 事件系统基于Boost.Asio的io_context和executor_work_guard实现异步事件处理。
 * 它支持事件的注册、注销和触发，以及事件监听器的添加和移除。
 */ 
#ifndef EVENT_SYSTEM_H
#define EVENT_SYSTEM_H

#include <functional>
#include <memory>
#include <queue>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <iostream>
#include <string>
#include <algorithm>
#include <chrono>

namespace im {
namespace network {

/**
 * @class Event
 * @brief 事件基类，所有事件都必须继承自该类
 * 
 * 事件类定义了事件的基本接口，所有具体的事件类型都必须实现handle()方法。
 * 事件循环会调用事件的handle()方法来处理事件。
 */ 
class Event {
public:
    virtual ~Event() = default;

    // 获取事件类型的唯一标识
    // std::type_index 类型索引工具，将 std::type_info 封装为可哈希的类型，让事件类型可以作为键值保存在 std::unordered_map 中
    virtual std::type_index getType() const = 0;
    // 获取事件名称（用于调试）
    virtual std::string getName() const = 0;

    // 事件优先级（数值越大优先级越高）
    int priority = 0;
    // 事件时间戳
    std::chrono::steady_clock::time_point timestamp;

    Event() : timestamp(std::chrono::steady_clock::now()) {}
};

/**
 * @struct EventQueueItem
 * @brief 事件队列项
 * 
 * 用于事件优先级队列，根据事件优先级排序
 */ 
struct EventQueueItem {
    std::shared_ptr<Event> event;

    EventQueueItem(std::shared_ptr<Event> e) : event(std::move(e)) {}

    // 优先级比较（优先级高的排在前面）
    bool operator<(const EventQueueItem& other) const {
        return event->priority < other.event->priority;
    }
};

/**
 * @class EventListenerBase
 * @brief 事件监听器基类
 * 
 * 类型擦除：不带模板参数的基类可以在容器 vector 中存储不同类型的事件监听器
 */ 
class EventListenerBase {
public:
    virtual ~EventListenerBase() = default;
    virtual void onEvent(const std::shared_ptr<Event>& event) = 0;

    // 监听器ID（用于注销）
    size_t listenerId = 0;
};

/**
 * @class EventListener
 * @brief 具体事件监听器模板类
 * 
 */ 
template<typename T>
class EventListener : public EventListenerBase {
public:
    // 事件回调函数类型，使用 std::function 封装
    // 可以接受函数指针，lambda 表达式，或绑定的函数对象，成员函数（通过 std::bind 绑定或 lambda 捕获 this 指针）
    using CallbackType = std::function<void(const std::shared_ptr<T>&)>;

    explicit EventListener(CallbackType callback)
        : callback_(std::move(callback)) {}

    void onEvent(const std::shared_ptr<Event>& event) override {
        // 类型安全的向下转换，运行时检查事件类型是否匹配，转换失败会返回 nullptr
        auto typedEvent = std::dynamic_pointer_cast<T>(event);
        if (typedEvent && callback_) {
            callback_(typedEvent);
        }
    }

private:
    CallbackType callback_;
};

class EventSystem
{
public:
    /**
     * @brief 获取单例实例
     * 
     * @return EventSystem& 事件系统单例实例
     */
    static EventSystem& getInstance();

    /**
     * @brief 禁止拷贝和赋值，确保单例的唯一性
     */
    EventSystem(const EventSystem&) = delete;
    EventSystem& operator=(const EventSystem&) = delete;

    ~EventSystem();

    /**
     * @brief 启动事件系统，开始异步处理事件
     */
    void start();

    /**
     * @brief 停止事件系统，等待所有事件处理完成
     */
    void stop();

    /**
     * @brief 注册事件监听器
     * 
     * @tparam T 事件类型
     * @param callback 事件回调函数
     * @return size_t 监听器ID（用于注销）
     */
    template<typename T>
    size_t subscribe(typename EventListener<T>::CallbackType callback);

    /**
     * @brief 注销事件监听器
     * 
     * @tparam T 事件类型
     * @param listenerId 监听器ID
     * @return true 注销成功
     * @return false 注销失败（ID不存在）
     */
    template<typename T>
    bool unsubscribe(size_t listenerId);

    /**
     * @brief 发布事件（异步）
     * 
     * @tparam T 事件类型
     * @param event 事件对象
     * @note 优先级只对队列中的事件有效。如果需要确保多个事件按优先级处理，
     *       请使用 publishBatch() 方法批量发布。
     */
    template<typename T>
    void publish(std::shared_ptr<T> event);

    /**
     * @brief 批量发布事件（异步）
     * 
     * @param events 事件对象列表
     * @note 确保所有事件都入队后再唤醒处理线程
     *       当需要发布多个相关事件时，使用此方法确保优先级正确生效
     */
    void publishBatch(std::vector<std::shared_ptr<Event>> events);

    /**
     * @brief 同步处理事件（立即执行）
     * 
     * @tparam T 事件类型
     * @param event 事件对象
     * @note 此方法会阻塞调用线程，直到事件处理完成
     */
    template<typename T>
    void dispatch(std::shared_ptr<T> event);

    /**
     * @brief 获取当前待处理事件数量
     * 
     * @return size_t 待处理事件数量
     */
    size_t getPendingEventCount() const;

    /**
     * @brief 清空事件队列
     * 
     */
    void clearQueue();
private:
    EventSystem();

    /**
     * @brief 事件处理线程函数
     * 
     */
    void processEvents();

    // 监听器映射表（事件类型 -> 监听器列表）
    std::unordered_map<std::type_index, std::vector<std::shared_ptr<EventListenerBase>>> listeners_;

    // 事件优先级队列
    std::priority_queue<EventQueueItem> eventQueue_;

    // 线程同步，允许监听器和事件发布并发
    std::mutex mutex_;           // 保护监听器列表
    std::mutex queueMutex_;      // 保护事件队列
    std::condition_variable cv_; // 条件变量
    std::thread workerThread_;   // 工作线程
    std::atomic<bool> running_;  // 运行状态，原子读写

    // 监听器ID生成器
    size_t nextListenerId_;

    // 待处理事件计数
    std::atomic<size_t> eventCount_;
};


} // namespace network
} // namespace im

#endif // EVENT_SYSTEM_H
