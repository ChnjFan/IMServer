#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <string>

#include "gateway_routing.pb.h"

namespace routing {

/**
 * @brief 消息队列项类
 * 表示队列中的一个消息
 */
class QueueItem {
public:
    im::common::protocol::RouteRequest request;
    std::function<void(const im::common::protocol::RouteResponse&)> callback;
    int priority;
    int64_t timestamp;
    
    QueueItem() : priority(0), timestamp(0) {}
    
    QueueItem(im::common::protocol::RouteRequest req, std::function<void(const im::common::protocol::RouteResponse&)> cb, int p)
        : request(std::move(req)),
          callback(std::move(cb)),
          priority(p),
          timestamp(std::chrono::system_clock::now().time_since_epoch().count()) {}
    
    // 优先级比较运算符
    bool operator<(const QueueItem& other) const {
        // 优先级高的排在前面
        if (priority != other.priority) {
            return priority < other.priority;
        }
        // 时间戳小的排在前面（先到先服务）
        return timestamp > other.timestamp;
    }
};

/**
 * @brief 消息队列类
 * 负责管理和处理消息队列
 */
class MessageQueue {
private:
    // 消息队列（优先队列）
    std::priority_queue<QueueItem> queue_;
    // 队列锁
    std::mutex queue_mutex_;
    // 条件变量
    std::condition_variable cv_;
    // 队列最大长度
    size_t max_size_;
    // 队列当前长度
    std::atomic<size_t> current_size_;
    // 停止标志
    std::atomic<bool> stop_;
    // 处理线程数
    int thread_count_;
    // 处理线程
    std::vector<std::thread> workers_;

public:
    MessageQueue();
    ~MessageQueue();

    /**
     * @brief 启动消息队列
     * @param thread_count 处理线程数
     */
    void start(int thread_count = 4);

    /**
     * @brief 停止消息队列
     */
    void stop();

    /**
     * @brief 入队消息
     * @param item 消息项
     * @return 是否入队成功
     */
    bool enqueue(const QueueItem& item);

    /**
     * @brief 入队消息
     * @param request 路由请求
     * @param callback 回调函数
     * @param priority 优先级
     * @return 是否入队成功
     */
    bool enqueue(const im::common::protocol::RouteRequest& request, 
                 std::function<void(const im::common::protocol::RouteResponse&)> callback, 
                 int priority = 0);

    /**
     * @brief 出队消息
     * @param item 消息项
     * @return 是否出队成功
     */
    bool dequeue(QueueItem& item);

    /**
     * @brief 获取队列大小
     * @return 队列大小
     */
    size_t size() const { return current_size_; }

    /**
     * @brief 获取队列最大长度
     * @return 队列最大长度
     */
    size_t maxSize() const { return max_size_; }

    /**
     * @brief 设置队列最大长度
     * @param size 最大长度
     */
    void setMaxSize(size_t size) { max_size_ = size; }

    /**
     * @brief 清空队列
     */
    void clear();

    /**
     * @brief 处理消息的回调函数类型
     */
    using MessageProcessor = std::function<im::common::protocol::RouteResponse(const im::common::protocol::RouteRequest&)>;

    /**
     * @brief 设置消息处理器
     * @param processor 消息处理器
     */
    void setProcessor(MessageProcessor processor);

private:
    // 消息处理器
    MessageProcessor processor_;

    /**
     * @brief 工作线程函数
     */
    void workerFunction();
};

} // namespace routing
