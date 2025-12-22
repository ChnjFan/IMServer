
#include "EventSystem.h"

namespace im {
namespace network {

EventSystem& EventSystem::getInstance() {
    static EventSystem instance;
    return instance;
}

EventSystem::~EventSystem() {
    stop();
}

void EventSystem::start() {
    if (running_.load()) {
        std::cout << "[EventSystem] 已经在运行中" << std::endl;
        return;
    }

    running_.store(true);
    workerThread_ = std::thread(&EventSystem::processEvents, this);
    std::cout << "[EventSystem] 异步处理线程已启动" << std::endl;
}

void EventSystem::stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);
    cv_.notify_all();
    // 等待线程完成后再结束，避免线程泄露导致程序退出异常
    if (workerThread_.joinable()) {
        workerThread_.join();
    }

    std::cout << "[EventSystem] 异步处理线程已停止" << std::endl;
}

template<typename T>
size_t EventSystem::subscribe(typename EventListener<T>::CallbackType callback) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 移动语义：将回调函数移动到新创建的 EventListener 中，避免拷贝
    auto listener = std::make_shared<EventListener<T>>(std::move(callback));
    listener->listenerId = nextListenerId_++;

    std::type_index typeIndex(typeid(T));
    listeners_[typeIndex].push_back(listener);

    std::cout << "[EventSystem] 注册监听器 ID=" << listener->listenerId
                << " 事件类型=" << typeIndex.name() << std::endl;

    return listener->listenerId;
}

template<typename T>
bool EventSystem::unsubscribe(size_t listenerId) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::type_index typeIndex(typeid(T));
    auto it = listeners_.find(typeIndex);

    if (it == listeners_.end()) {
        return false;
    }

    auto& listenerList = it->second;
    // 使用 remove_if 移除匹配的监听器，防止迭代器失效
    // std::remove_if 不会直接删除元素（不改变容器大小），只是「标记」要删除的元素（移到尾部）并返回待删除的第一个元素
    // 后续需配合 erase 才能真正删除。
    auto listenerIt = std::remove_if(listenerList.begin(), listenerList.end(),
        [listenerId](const std::shared_ptr<EventListenerBase>& listener) {
            return listener->listenerId == listenerId;
        });

    if (listenerIt != listenerList.end()) {
        // 移除[listenerIt, listenerList.end()) 范围内的元素
        listenerList.erase(listenerIt, listenerList.end());
        std::cout << "[EventSystem] 注销监听器 ID=" << listenerId << std::endl;
        return true;
    }

    return false;
}

template<typename T>
void EventSystem::publish(std::shared_ptr<T> event) {
    // std::is_base_of<Event, T>::value 检查 T 是否是 Event 的子类
    static_assert(std::is_base_of<Event, T>::value,
                    "T must inherit from Event");

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        eventQueue_.push(EventQueueItem(event));
        // 原子执行：先返回 count 旧值（0），再将 count 加 1（最终 count=1）
        eventCount_.fetch_add(1);
    }

    // 唤醒处理线程
    cv_.notify_one();

    std::cout << "[EventSystem] 发布事件: " << event->getName()
                << " 优先级=" << event->priority << std::endl;
}

void EventSystem::publishBatch(std::vector<std::shared_ptr<Event>> events) {
    if (events.empty()) return;

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        for (auto& event : events) {
            eventQueue_.push(EventQueueItem(event));
            eventCount_.fetch_add(1);
            std::cout << "[EventSystem] 批量发布事件: " << event->getName()
                        << " 优先级=" << event->priority << std::endl;
        }
    }

    // 只在所有事件入队后唤醒一次
    cv_.notify_one();
}

template<typename T>
void EventSystem::dispatch(std::shared_ptr<T> event) {
    static_assert(std::is_base_of<Event, T>::value,
                    "T must inherit from Event");

    std::cout << "[EventSystem] 同步分发事件: " << event->getName() << std::endl;

    std::lock_guard<std::mutex> lock(mutex_);

    std::type_index typeIndex(typeid(T));
    auto it = listeners_.find(typeIndex);

    if (it != listeners_.end()) {
        for (auto& listener : it->second) {
            listener->onEvent(event);
        }
    }
}

size_t EventSystem::getPendingEventCount() const {
    return eventCount_.load();
}

void EventSystem::clearQueue() {
    std::lock_guard<std::mutex> lock(queueMutex_);
    while (!eventQueue_.empty()) {
        eventQueue_.pop();
    }
    eventCount_.store(0);
    std::cout << "[EventSystem] 事件队列已清空" << std::endl;
}

EventSystem::EventSystem() : running_(false), nextListenerId_(1), eventCount_(0) {}

void EventSystem::processEvents() {
    std::cout << "[EventSystem] 事件处理线程开始运行" << std::endl;

    while (running_.load()) {
        std::shared_ptr<Event> event;

        {
            std::unique_lock<std::mutex> lock(queueMutex_);

            // 等待事件或停止信号，解决虚假唤醒问题
            // 1.加锁后检查当事件队列不空或停止时，立即返回处理
            // 2.如果事件队列空且未停止，释放锁后等待 notify_one 被唤醒
            // 3.被唤醒后再次检查事件队列是否为空，若为空则继续等待
            cv_.wait(lock, [this] {
                return !eventQueue_.empty() || !running_.load();
            });

            // 确保退出前处理完所有事件
            if (!running_.load() && eventQueue_.empty()) {
                break;
            }

            if (!eventQueue_.empty()) {
                event = eventQueue_.top().event;//获取堆顶元素
                eventQueue_.pop();
                eventCount_.fetch_sub(1);
            }
        }

        // 分发事件
        if (event) {
            // 复制监听器列表（避免在持有锁时调用回调导致死锁）
            // 如果在持有锁 mutex_ 时直接调用事件监听回调
            // 事件监听回调可能还会执行发布事件、注册监听回调等操作尝试获取 mutex_ 导致死锁
            std::vector<std::shared_ptr<EventListenerBase>> listenersCopy;

            {
                std::lock_guard<std::mutex> lock(mutex_);

                std::type_index typeIndex = event->getType();
                auto it = listeners_.find(typeIndex);

                if (it != listeners_.end()) {
                    std::cout << "[EventSystem] 处理事件: " << event->getName()
                                << " 监听器数量=" << it->second.size() << std::endl;

                    // 复制监听器列表然后释放锁，避免在持有锁时调用回调导致死锁
                    listenersCopy = it->second;
                }
            }  // 锁在这里释放

            // 在锁外调用监听器回调（安全，不会死锁）
            for (auto& listener : listenersCopy) {
                try {
                    listener->onEvent(event);
                } catch (const std::exception& e) {
                    std::cerr << "[EventSystem] 监听器异常: " << e.what() << std::endl;
                }
            }
        }
    }

    std::cout << "[EventSystem] 事件处理线程结束" << std::endl;
}


} // namespace network
} // namespace im