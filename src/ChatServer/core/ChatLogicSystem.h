//
// Created by Fan on 2026/5/12.
//

#ifndef IMSERVER_CHATLOGICSYSTEM_H
#define IMSERVER_CHATLOGICSYSTEM_H

#include <thread>
#include <atomic>
#include <functional>
#include <string>
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <boost/lockfree/queue.hpp>

#include <json/json.h>

#include "const.h"
#include "Singleton.h"
#include "MsgNode.h"
#include "MysqlMgr.h"
#include "ThreadPool.h"
#include "common/model/UserBaseInfo.h"
#include "core/ChatMsgNode.h"

/**
 * @brief ChatLogicSystem 的性能统计聚合。
 *
 * 所有方法线程安全，可由多个 worker 线程并发调用。
 * 统计分为两类：
 *   - 累计值（total_*）：从启动开始累加，永不重置，用于长期观测。
 *   - 周期值（handler_* / total_idle_count_）：每次 printStats 后清零，
 *     反映最近一个周期内的分布。
 */
class PerfStats {
public:
    void recordMessage(uint64_t queue_wait_us, uint64_t process_us, uint16_t msgId);

    void recordIdle(uint64_t idle_us);

    /// 打印汇总统计并重置周期值。返回从上次打印经过的秒数。
    double printStats(const std::chrono::steady_clock::time_point& now);

    uint64_t totalMessages() const;

    /// 返回距上次打印经过的秒数（不修改状态）。
    double elapsedSinceReport(const std::chrono::steady_clock::time_point& now) const;

private:
    mutable std::mutex mutex_;

    // ── 累计值 ──────────────────────────────────────────────
    uint64_t total_queue_wait_us_ = 0;   ///< 消息在队列中等待的总时间
    uint64_t total_process_us_ = 0;      ///< 业务处理的总时间
    uint64_t total_messages_ = 0;        ///< 处理的消息总数
    uint64_t total_idle_us_ = 0;         ///< worker 线程空闲总时间（休眠等待）

    // ── 周期值（每次 printStats 后清零）─────────────────────
    std::unordered_map<uint16_t, uint64_t> handler_process_us_; ///< 每个 msgId 的处理总时间
    std::unordered_map<uint16_t, uint64_t> handler_count_;      ///< 每个 msgId 的处理次数
    uint64_t total_idle_count_ = 0;      ///< worker 进入空闲休眠的次数

    std::chrono::steady_clock::time_point last_report_time_;     ///< 上次打印的时间点
};

typedef std::function<void(std::shared_ptr<Session> session, const uint16_t msgId, const std::string& data)> msgHandler;

class BatchWriter;
typedef std::function<void(const std::string& serviceName)> notifyOnlineUserCallback;

/**
 * @brief 单个 Worker 分片：独立队列 + 独立条件变量 + 独立 Worker 线程。
 *
 * 多队列分片的核心数据结构。每个 shard 的 worker 线程只操作自己的
 * 队列和 condvar，消除多 worker 争用单一队列的 CAS 瓶颈。
 */
struct WorkerShard {
    /// 无锁队列：IO 线程 push、对应 worker 线程 pop
    /// 容量与单队列方案保持一致（2048），总容量 = 2048 × shard 数
    boost::lockfree::queue<std::shared_ptr<LogicNode>*> queue;

    /// mutex + cond 仅用于该 shard 的 worker 线程在无消息时休眠
    std::mutex mutex;
    std::condition_variable cond;

    /// 单 shard 队列容量
    static constexpr int SHARD_QUEUE_CAPACITY = 2048;

    WorkerShard() : queue(SHARD_QUEUE_CAPACITY) {}
    ~WorkerShard() {
        // 清理队列中的未处理消息
        std::shared_ptr<LogicNode>* nodePtr;
        while (queue.pop(nodePtr)) {
            delete nodePtr; // 释放指针
        }
    }

    // 不可拷贝、不可移动（含 mutex 成员）
    WorkerShard(const WorkerShard&) = delete;
    WorkerShard& operator=(const WorkerShard&) = delete;
};

class ChatLogicSystem : public Singleton<ChatLogicSystem> {
public:
    ~ChatLogicSystem();
    void close();

    void setServerName(const std::string& name);

    void insertMsgNode(const std::shared_ptr<LogicNode> &msg);

    void notifyOnlineUserMsg(int uid, const std::string& msg, MessageID msgId, const notifyOnlineUserCallback &callback);

private:
    friend class Singleton<ChatLogicSystem>;

    // 初始化聊天服务逻辑
    ChatLogicSystem();

    static int getIoWorkerNum();

    void initHandlers();
    void registerHandler(uint16_t msgId, const msgHandler& handler);
    // 处理消息（绑定到指定 shard）
    void dealMsg(size_t shard_idx);
    void handleMsgNode(const std::shared_ptr<LogicNode>& node);

    /// 根据 Session ID 哈希选择目标 shard
    size_t getShardIndex(const std::shared_ptr<LogicNode>& msg) const;

    // 客户端踢人逻辑
    void kickOnlineUser(int uid) const;
    // 登录逻辑
    void loginHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data) const;

    static int getApplyFriendCount(int uid);
    void firstPageInfoHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);

    // 搜索好友列表
    void searchFriendListHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);

    // 查询用户详细信息
    static void getSearchInfoFromJson(Json::Value& root, UserBaseInfo& userInfo);
    static bool searchUserFullInfo(UserBaseInfo& baseInfo, UserProfile& profile);
    static bool isFriend(int uid, int friendId);

    static void setFriendRelation(int uid, int friendId, Json::Value& root);
    void searchUserFullInfoHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);

    // 搜索好友用户
    static std::string getSearchKey(UserBaseInfo& userInfo);
    static bool searchUserBaseInfo(UserBaseInfo& userInfo);
    void searchUserHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);

    // 获取好友申请列表
    void searchFriendApplyListHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);
    // 好友申请
    static bool checkFriendApplyInvalid(int uid, int friendId);
    void friendApplyHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);
    // 认证好友
    void friendAuthHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);
    // 更新好友关系
    void updateFriendHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);

    // 修改用户信息
    void updateUserInfoHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);

    // 创建会话
    static bool isPrivateChat(int uid);
    static bool checkConversationValid(int uid, int other);
    void conversationCreateHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);

    static void getConversationTitleInfo(const ConversationInfo& convInfo, Json::Value& root);
    void conversationListFetchHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);

    // 聊天消息
    void chatMsgHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);
    void historyChatMsgFetchHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);
    void msgStatusUpdateHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);

    // 心跳包处理
    void heartbeatHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);


    // =============== 待修复 ===============

    void uploadFileHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);

    std::atomic<bool> stop_;
    std::vector<std::thread> workers_;

    std::string selfServerName_;

    // 多队列分片：每个 shard 拥有独立的 lockfree 队列和 condvar
    std::vector<std::unique_ptr<WorkerShard>> shards_;

    PerfStats stats_;

    ThreadPool workerPool_;

    // 批量异步写入
    std::unique_ptr<BatchWriter> batch_writer_;

    std::unordered_map<uint16_t, msgHandler> handlers_;
};


#endif //IMSERVER_CHATLOGICSYSTEM_H
