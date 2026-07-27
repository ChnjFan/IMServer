//
// Created by Fan on 2026/5/12.
//

#include <regex>

#include <json/value.h>
#include <json/reader.h>

#include "ChatLogicSystem.h"

#include "StatusGrpcClient.h"
#include "Session.h"
#include "RedisMgr.h"
#include "UserMgr.h"
#include "ChatGrpcClient.h"
#include "LogicWorker.h"
#include "BatchWriter.h"

#include "db/mysql/MysqlMgr.h"
#include "db/cache/UserInfoCache.h"
#include "db/cache/FriendCache.h"
#include "common/model/ConversationInfo.h"
#include "common/model/MessageInfo.h"

// ──────────────────────────────────────────────────────────────
// PerfStats
// ──────────────────────────────────────────────────────────────

void PerfStats::recordMessage(const uint64_t queue_wait_us, const uint64_t process_us, const uint16_t msgId) {
    std::lock_guard<std::mutex> lock(mutex_);
    total_queue_wait_us_ += queue_wait_us;
    total_process_us_ += process_us;
    total_messages_++;
    handler_process_us_[msgId] += process_us;
    handler_count_[msgId]++;
}

void PerfStats::recordIdle(const uint64_t idle_us) {
    std::lock_guard<std::mutex> lock(mutex_);
    total_idle_us_ += idle_us;
    total_idle_count_++;
}

double PerfStats::printStats(const std::chrono::steady_clock::time_point& now) {
    // 快照数据，尽快释放锁，避免阻塞 worker
    uint64_t total_messages, total_queue_wait, total_process, total_idle, total_idle_count;
    std::unordered_map<uint16_t, uint64_t> handler_process_us, handler_count;
    double elapsed_sec;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        total_messages = total_messages_;
        total_queue_wait = total_queue_wait_us_;
        total_process = total_process_us_;
        total_idle = total_idle_us_;
        total_idle_count = total_idle_count_;
        handler_process_us = handler_process_us_;
        handler_count = handler_count_;

        elapsed_sec = std::chrono::duration<double>(now - last_report_time_).count();
        last_report_time_ = now;

        // 重置周期值
        handler_process_us_.clear();
        handler_count_.clear();
        total_idle_count_ = 0;
    }

    double avg_queue = total_messages > 0 ? static_cast<double>(total_queue_wait) / total_messages : 0;
    double avg_process = total_messages > 0 ? static_cast<double>(total_process) / total_messages : 0;
    double avg_idle = total_messages > 0 ? static_cast<double>(total_idle) / total_messages : 0;
    double idle_ratio = total_messages > 0 ? static_cast<double>(total_idle_count) / total_messages * 100.0 : 0;

    // worker 利用率 = process / (process + idle)
    double utilization = (total_process + total_idle) > 0
        ? static_cast<double>(total_process) / (total_process + total_idle) * 100.0 : 0;

    std::cout << "[perf] total_msg=" << total_messages
              << " avg_queue_wait=" << avg_queue << "us"
              << " avg_process=" << avg_process << "us"
              << " avg_idle=" << avg_idle << "us"
              << " util=" << std::fixed << std::setprecision(1) << utilization << "%"
              << " idle_ratio=" << std::setprecision(1) << idle_ratio << "%"
              << " ratio(queue:process)="
              << (avg_process > 0 ? avg_queue / avg_process : 0) << ":1"
              << std::endl;

    // per-handler  breakdown (按总耗时降序)
    std::vector<std::pair<uint16_t, uint64_t>> handler_vec(handler_process_us.begin(), handler_process_us.end());
    std::sort(handler_vec.begin(), handler_vec.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    std::cout << "[perf_detail] ";
    for (const auto& [msgId, total_us] : handler_vec) {
        uint64_t count = handler_count[msgId];
        double avg_us = count > 0 ? static_cast<double>(total_us) / count : 0;
        std::cout << "msgId=" << msgId
                  << "{count=" << count
                  << " avg=" << std::fixed << std::setprecision(0) << avg_us << "us"
                  << " total=" << total_us / 1000 << "ms} ";
    }
    std::cout << std::endl;

    return elapsed_sec;
}

uint64_t PerfStats::totalMessages() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return total_messages_;
}

double PerfStats::elapsedSinceReport(const std::chrono::steady_clock::time_point& now) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::chrono::duration<double>(now - last_report_time_).count();
}

// ──────────────────────────────────────────────────────────────
// ChatLogicSystem
// ──────────────────────────────────────────────────────────────

ChatLogicSystem::~ChatLogicSystem() {
    close();
    for (auto& shard : shards_) {
        shard->cond.notify_all();
    }
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ChatLogicSystem::close() {
    stop_.store(true);
    if (batch_writer_) {
        batch_writer_->stop();
    }
}

void ChatLogicSystem::setServerName(const std::string &name) {
    selfServerName_ = name;
}

void ChatLogicSystem::insertMsgNode(const std::shared_ptr<LogicNode> &msg) {
    // 根据 Session ID 哈希选择目标 shard，保证同一 Session 的消息有序
    size_t idx = getShardIndex(msg);
    auto& shard = *shards_[idx];

    // 将 shared_ptr 拷贝到堆上，保证对象在队列中始终存活
    auto* heapPtr = new std::shared_ptr<LogicNode>(msg);
    // 无锁 push：IO 线程分散到不同 shard，大幅减少 CAS 争用
    while (!shard.queue.push(heapPtr)) {
        // 队列满（极少发生），自旋重试
        std::this_thread::yield();
    }
    // 唤醒目标 shard 的 worker 来处理新消息
    shard.cond.notify_one();
}

size_t ChatLogicSystem::getShardIndex(const std::shared_ptr<LogicNode> &msg) const {
    size_t hash = std::hash<std::string>{}(msg->session_->getSessionId());
    return hash % shards_.size();
}

void ChatLogicSystem::notifyOnlineUserMsg(const int uid, const std::string &msg, MessageID msgId,
    const notifyOnlineUserCallback &callback) {
    const auto toServiceName = RedisMgr::getInstance()->hGet(
        USER_ONLINE_INFO_PREFIX + std::to_string(uid), USER_ONLINE_SERVER_NAME);
    if (toServiceName.empty()) {
        return;// 用户不在线直接返回，等到上线直接从数据库拉取
    }

    // 同一服务器直接发送申请消息
    if (toServiceName == selfServerName_) {
        if (const auto toSession = UserMgr::getInstance()->getSession(uid)) {
            toSession->asyncSend(msg, static_cast<std::uint16_t>(msgId));
        }
        return;
    }

    return callback(toServiceName);
}

ChatLogicSystem::ChatLogicSystem()
    : stop_(false), workerPool_() {
    initHandlers();
    // 创建 N 个 shard，每个 shard 拥有独立的 lockfree 队列和 condvar
    int numWorkers = getIoWorkerNum();
    shards_.reserve(numWorkers);
    for (int i = 0; i < numWorkers; ++i) {
        shards_.push_back(std::make_unique<WorkerShard>());
    }
    // 每个 worker 线程绑定一个 shard，消除多 worker 争用单一队列的 CAS 瓶颈
    for (int i = 0; i < numWorkers; ++i) {
        workers_.emplace_back(&ChatLogicSystem::dealMsg, this, i);
    }
    workerPool_.start();

    // 初始化批量写入管理器
    {
        size_t numShards = shards_.size();
        size_t numWriters = std::max<size_t>(1, numShards / 4);
        batch_writer_ = std::make_unique<BatchWriter>(numShards, numWriters);
        batch_writer_->start();
    }

    // 触发一次打印以初始化 stats_ 的内部时间戳（避免首条消息 elapsed 极大）
    stats_.printStats(std::chrono::steady_clock::now());
}

int ChatLogicSystem::getIoWorkerNum() {
    constexpr int MIN_WORKERS = 4;
    constexpr int MAX_WORKERS = 256;
    const auto core = std::thread::hardware_concurrency();
    const int num = core * 3 / 2;
    return num < MIN_WORKERS ? MIN_WORKERS : (num > MAX_WORKERS ? MAX_WORKERS : num);
}

void ChatLogicSystem::initHandlers() {
    registerHandler(static_cast<uint16_t>(MessageID::ID_CHAT_LOGIN),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return loginHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_FIRST_PAGE_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return firstPageInfoHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_GET_FRIEND_LIST_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return searchFriendListHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_USER_SEARCH_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return searchUserHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_GET_USER_FULL_INFO_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return searchUserFullInfoHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_FRIEND_APPLY_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return friendApplyHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_FRIEND_AUTH_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return friendAuthHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_GET_FRIEND_REPLY_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return searchFriendApplyListHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_UPDATE_FRIEND_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return updateFriendHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_UPDATE_USERINFO_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return updateUserInfoHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_CHAT_CONVERSATION_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return conversationCreateHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_CONV_LIST_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return conversationListFetchHandle(session, msgId, data);
        });

    registerHandler(static_cast<uint16_t>(MessageID::ID_CHAT_MSG_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return chatMsgHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_CONV_HISTORY_MSG_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return historyChatMsgFetchHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_CONV_MSG_UPDATE_STATUS_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return msgStatusUpdateHandle(session, msgId, data);
        });
    registerHandler(static_cast<uint16_t>(MessageID::ID_HEART_BEAT_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return heartbeatHandle(session, msgId, data);
        });

    registerHandler(static_cast<uint16_t>(MessageID::ID_CHAT_UPLOAD_FILE_REQ),
        [this](const std::shared_ptr<Session> &session, const uint16_t msgId, const std::string& data) {
            return uploadFileHandle(session, msgId, data);
        });
}

void ChatLogicSystem::registerHandler(uint16_t msgId, const msgHandler& handler) {
    if (handlers_.find(msgId) != handlers_.end()) {
        return;
    }
    handlers_.insert({msgId, handler});
}

void ChatLogicSystem::dealMsg(size_t shard_idx) {
    auto& shard = *shards_[shard_idx];

    while (true) {
        std::shared_ptr<LogicNode>* heapPtr = nullptr;

        // 快速路径：无锁 pop，只操作本 shard 的队列
        if (shard.queue.pop(heapPtr)) {
            // 取出 shared_ptr 并释放堆包装
            std::shared_ptr<LogicNode> msgNode = *heapPtr;
            delete heapPtr;

            msgNode->handle_start_time = std::chrono::steady_clock::now();

            auto process_start = std::chrono::steady_clock::now();
            handleMsgNode(msgNode);
            auto process_end = std::chrono::steady_clock::now();

            // 计算排队时间和处理时间
            auto queue_wait_us = std::chrono::duration_cast<std::chrono::microseconds>(
                msgNode->handle_start_time - msgNode->recv_time).count();
            auto process_us = std::chrono::duration_cast<std::chrono::microseconds>(
                process_end - process_start).count();

            // 累加聚合统计
            stats_.recordMessage(
                static_cast<uint64_t>(std::max(0LL, queue_wait_us)),
                static_cast<uint64_t>(std::max(0LL, process_us)),
                msgNode->node_->msgId_);

            // 每 1 秒或每 10000 条打印一次聚合统计
            auto now = std::chrono::steady_clock::now();
            if (stats_.elapsedSinceReport(now) >= 1.0 || stats_.totalMessages() % 10000 == 0) {
                stats_.printStats(now);
                if (batch_writer_) batch_writer_->printMetrics();
            }
            continue;
        }

        // 慢速路径：队列空，worker 休眠等待（避免空转 CPU）
        {
            std::unique_lock<std::mutex> lock(shard.mutex);
            // 再次检查（防止 push 在 pop 和 wait 之间到达）
            if (shard.queue.pop(heapPtr)) {
                lock.unlock();
                // 取出 shared_ptr 并释放堆包装
                std::shared_ptr<LogicNode> msgNode = *heapPtr;
                delete heapPtr;

                msgNode->handle_start_time = std::chrono::steady_clock::now();
                auto process_start = std::chrono::steady_clock::now();
                handleMsgNode(msgNode);
                auto process_end = std::chrono::steady_clock::now();
                auto queue_wait_us = std::chrono::duration_cast<std::chrono::microseconds>(
                    msgNode->handle_start_time - msgNode->recv_time).count();
                auto process_us = std::chrono::duration_cast<std::chrono::microseconds>(
                    process_end - process_start).count();
                stats_.recordMessage(
                    static_cast<uint64_t>(std::max(0LL, queue_wait_us)),
                    static_cast<uint64_t>(std::max(0LL, process_us)),
                    msgNode->node_->msgId_);
                continue;
            }
            if (stop_.load()) {
                // 关闭前处理本 shard 剩余消息
                while (shard.queue.pop(heapPtr)) {
                    std::shared_ptr<LogicNode> msgNode = *heapPtr;
                    delete heapPtr;
                    handleMsgNode(msgNode);
                }
                break;
            }
            // 真正空闲，休眠等待唤醒（只等待本 shard 的 condvar）
            // 记录开始等待的时间，用于统计 idle 时长
            auto idle_start = std::chrono::steady_clock::now();
            shard.cond.wait_for(lock, std::chrono::milliseconds(1), [this]() {
                return stop_.load();
            });
            auto idle_end = std::chrono::steady_clock::now();
            auto idle_us = std::chrono::duration_cast<std::chrono::microseconds>(
                idle_end - idle_start).count();
            stats_.recordIdle(static_cast<uint64_t>(std::max(0LL, idle_us)));
        }
    }
}

void ChatLogicSystem::handleMsgNode(const std::shared_ptr<LogicNode> &node) {
    node->session_->updateLstActiveTime();

    if (handlers_.find(node->node_->msgId_) == handlers_.end()) {
        std::cout << "Msg id [" << node->node_->msgId_ << "] handler not found" << std::endl;
        Json::Value msg;
        msg["error"] = static_cast<int32_t>(ErrorCodes::REQUEST_NOT_FOUND);
        node->session_->asyncSend(msg.toStyledString(), static_cast<uint16_t>(MessageID::ID_CLIENT_COMMON_RSP));
        return;
    }
    try {
        handlers_[node->node_->msgId_](node->session_, node->node_->msgId_, std::string(node->node_->buffer_));
    } catch (...) {
        std::cout << "Handle msg [" << node->node_->msgId_ << "] not found!" << std::endl;
        Json::Value msg;
        msg["error"] = static_cast<int32_t>(ErrorCodes::REQUEST_NOT_FOUND);
        node->session_->asyncSend(msg.toStyledString(), node->node_->msgId_ + 1);
    }
}

void ChatLogicSystem::kickOnlineUser(const int uid) const {
    const std::string serverName = RedisMgr::getInstance()->hGet(
        USER_ONLINE_INFO_PREFIX + std::to_string(uid),USER_ONLINE_SERVER_NAME);
    if (serverName.empty()) {
        return;
    }
    if (selfServerName_ == serverName) {// 用户在本服务器
        if (const auto oldSession = UserMgr::getInstance()->getSession(uid)) {
            oldSession->notifyOffline();
        }
    }
    else {// 用户在其他服务器，通知对端离线
        ChatGrpcClient::getInstance()->NotifyOffline(serverName, uid);
    }
}

void ChatLogicSystem::loginHandle(const std::shared_ptr<Session> &session, const uint16_t msgId,
                                  const std::string &data) const {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_CHAT_LOGIN_RSP));
    });
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
    const auto uid = srcRoot["uid"].asString();
    const int userid = std::stoi(uid);
    const auto reply = StatusGrpcClient::getInstance()->Login(userid, srcRoot["token"].asString());
    if (reply.error() != static_cast<int32_t>(ErrorCodes::SUCCESS)
        || reply.token() != srcRoot["token"].asString()) {
        std::cout << "Login token error, expect: " << reply.token() << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::CHAT_LOGIN_TOKEN_ERROR);
        return;
    }

    // 查询用户是否存在，返回基本信息
    UserBaseInfo userInfo;
    userInfo.uid = userid;
    if (!searchUserBaseInfo(userInfo)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::CHAT_LOGIN_UID_ERROR);
        return;
    }

    userInfo.toJson(root);
    root["token"] = reply.token();

    // 服务端踢人逻辑，将其他在线客户端下线
    kickOnlineUser(userid);
    // Session 与 uid 绑定
    session->setUserId(userid);
    UserMgr::getInstance()->setUserSession(userid, session);
    session->updateState(SessionState::ONLINE);
}

int ChatLogicSystem::getApplyFriendCount(const int uid) {
    int count = 0;
    if (FriendCache::getInstance()->getFriendApplyCount(uid, count)) {
        return count;
    }

    if (!MysqlMgr::getInstance()->getFriendApplyCount(uid, count)) {
        return 0;
    }

    if (!FriendCache::getInstance()->updateFriendApplyCount(uid, count)) {
        std::cout << "Friend apply count error" << std::endl;
    }

    return count < 0 ? 0 : count;
}

void ChatLogicSystem::firstPageInfoHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
                                          const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_FIRST_PAGE_RSP));
    });
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
    const auto uid = std::stoi(srcRoot["uid"].asString());
    // 获取未处理的好友申请计数
    root["friend_apply_count"] = getApplyFriendCount(uid);
}

void ChatLogicSystem::searchFriendListHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
                                             const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_GET_FRIEND_LIST_RSP));
    });
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);

    auto sinceTime = srcRoot["since_update_time"].asString();
    if (sinceTime.empty()) {
        sinceTime = "0000-00-00 00:00:00";
    }
    const auto uid = std::stoi(srcRoot["uid"].asString());
    if (uid < 0) {
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }

    const auto friendList = MysqlMgr::getInstance()->selectFriendList(uid, sinceTime);
    if (friendList.empty()) {
        root["error"] = static_cast<int32_t>(ErrorCodes::FRIEND_NOT_EXISTS);
    }

    for (const auto& friendInfo : friendList) {
        Json::Value friendJson;
        friendInfo.toJson(friendJson);
        root["data"].append(friendJson);
    }
}

void ChatLogicSystem::getSearchInfoFromJson(Json::Value &root, UserBaseInfo &userInfo) {
    if (root.isMember("uid")) {
        userInfo.uid = std::stoi(root["uid"].asString());
    }
    if (root.isMember("email")) {
        userInfo.email = root["email"].asString();
    }
    if (root.isMember("name")) {
        userInfo.name = root["name"].asString();
    }
}

bool ChatLogicSystem::searchUserFullInfo(UserBaseInfo &baseInfo, UserProfile& profile) {
    // Redis 缓存直接通过 uid 查询
    if (UserInfoCache::searchUserFullInfo(baseInfo, profile)) {
        return true;
    }

    if (!MysqlMgr::getInstance()->selectUserFullInfo(baseInfo, profile)) {
        std::cout << "Not found user by uid " << baseInfo.uid << std::endl;
        return false;
    }
    // 更新缓存
    Json::Value baseInfoRoot;
    baseInfo.toJson(baseInfoRoot);
    RedisMgr::getInstance()->set(USER_BASE_INFO_PREFIX + std::to_string(baseInfo.uid),
        baseInfoRoot.toStyledString());
    Json::Value profileInfoRoot;
    profile.toJson(profileInfoRoot);
    RedisMgr::getInstance()->set(USER_PROFILE_INFO_PREFIX + std::to_string(baseInfo.uid),
        profileInfoRoot.toStyledString());

    return true;
}

bool ChatLogicSystem::isFriend(const int uid, const int friendId) {
    if (FriendCache::getInstance()->isFriend(uid, friendId)) {
        return true;
    }

    if (!MysqlMgr::getInstance()->isFriendExist(uid, friendId)) {
        std::cout << "Not found user " << uid << " friend by uid " << friendId << std::endl;
        return false;
    }

    // 更新好友关系集合缓存
    if (!FriendCache::getInstance()->updateFriendSet(uid, friendId)) {
        std::cout << "update friend relation failed" << std::endl;
    }

    return true;
}

void ChatLogicSystem::setFriendRelation(const int uid, const int friendId, Json::Value &root) {
    if (friendId == uid || isFriend(uid, friendId)) {
        root["friend_status"] = static_cast<uint8_t>(FriendStatus::FRIEND_PRESENT);
        return;
    }

    root["friend_status"] = static_cast<uint8_t>(FriendStatus::NOT_FRIEND);
}

void ChatLogicSystem::searchUserFullInfoHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
                                               const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_GET_USER_FULL_INFO_RSP));
    });
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);

    UserBaseInfo baseInfo;
    UserProfile profile;
    getSearchInfoFromJson(srcRoot, baseInfo);
    if (!searchUserFullInfo(baseInfo, profile)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    baseInfo.toJson(root);
    profile.toJson(root);

    // 设置好友关系
    int from = -1;
    if (srcRoot.isMember("from")) {
        from = std::stoi(srcRoot["from"].asString());
    }
    setFriendRelation(from, baseInfo.uid, root);

    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
}


std::string ChatLogicSystem::getSearchKey(UserBaseInfo &userInfo) {
    if (userInfo.email.has_value()) {
        return userInfo.email.value();
    }
    if (userInfo.name.has_value()) {
        return userInfo.name.value();
    }
    return "";
}

bool ChatLogicSystem::searchUserBaseInfo(UserBaseInfo& userInfo) {
    if (UserInfoCache::searchUserBaseInfo(userInfo)) {
        return true;
    }
    // 缓存没有映射关系，只能去数据库查询
    if (!MysqlMgr::getInstance()->selectUserBaseInfo(userInfo) || userInfo.uid < 0) {
        return false;
    }
    // 更新缓存
    if (!UserInfoCache::updateBaseInfo(userInfo)) {
        std::cout << "Failed to update user info" << std::endl;
    }
    if (!UserInfoCache::updateUidMap(userInfo)) {
        std::cout << "Failed to update uid map" << std::endl;
    }
    return true;
}

void ChatLogicSystem::searchUserHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
                                       const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_USER_SEARCH_RSP));
    });
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }

    UserBaseInfo searchInfo;
    searchInfo.fromJson(srcRoot);
    if (!searchUserBaseInfo(searchInfo)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }

    // ReSharper disable once CppDFAConstantConditions
    if (searchInfo.uid < 0) {
        root["error"] = static_cast<int32_t>(ErrorCodes::USER_NOT_EXISTS);
        return;
    }

     searchInfo.toJson(root);
}

void ChatLogicSystem::searchFriendApplyListHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
                                                  const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_GET_FRIEND_REPLY_RSP));
    });
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
    const auto uid = std::stoi(srcRoot["uid"].asString());
    auto sinceTime = srcRoot["since_update_time"].asString();
    if (sinceTime.empty()) {
        sinceTime = "0000-00-00 00:00:00";
    }
    const std::vector<FriendApply> searchResult = MysqlMgr::getInstance()->selectFriendApplyList(uid, sinceTime);
    if (searchResult.empty()) {
        root["error"] = static_cast<int32_t>(ErrorCodes::FRIEND_APPLY_NOT_EXISTS);
        return;
    }

    for (auto& searchInfo : searchResult) {
        Json::Value info;
        searchInfo.toJson(info);
        root["data"].append(info);
    }

}

bool ChatLogicSystem::checkFriendApplyInvalid(const int uid, const int friendId) {
    // 检查是否是好友
    if (FriendCache::getInstance()->isFriend(uid, friendId)) {
        return true;
    }

    // todo 逻辑检查应该放在 SQL 中，入库时查看是否已经有记录
    // // 检查对方已经是好友，直接恢复好友关系，恢复失败走普通好友申请
    // if (checkFriendRelation(friendId, uid)) {
    //     FriendInfo info;
    //     info.friendId = friendId;
    //     info.status = static_cast<int8_t>(FriendStatus::FRIEND_PRESENT);
    //     if (!MysqlMgr::getInstance()->updateFriendRelation(uid, info)) {
    //         return false;
    //     }
    //     return true;
    // }
    //
    // // 检查是否已经存在提交记录
    // if (MysqlMgr::getInstance()->checkFriendApplyExist(uid, friendId)) {
    //     return true;
    // }

    return false;
}

void ChatLogicSystem::friendApplyHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
                                        const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_FRIEND_APPLY_RSP));
    });
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
    // 申请参数校验：是否好友、是否已经有存在的申请防止重复提交
    const auto from = std::stoi(srcRoot["uid"].asString());
    const auto to = std::stoi(srcRoot["friend_id"].asString());
    if (checkFriendApplyInvalid(from, to)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::USER_IS_FRIEND_RELATION);
        return;
    }

    // 保存申请记录
    std::string msg;
    if (srcRoot.isMember("message")) {
        msg = srcRoot["message"].asString();
    }
    if (!MysqlMgr::getInstance()->updateFriendApply(from, to, 0, msg)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::MYSQL_ERROR);
        return;
    }
    FriendCache::getInstance()->clearFriendApplyCount(to);

    // 通知在线用户
    notifyOnlineUserMsg(to, data, MessageID::ID_NOTIFY_FRIEND_APPLY,
            [from, to, &data](const std::string& serverName) {
        // 不同服务器调用 grpc 请求
        ChatServiceReq request;
        request.set_from_uid(from);
        request.set_to_uid(to);
        request.set_json(data);
        ChatGrpcClient::getInstance()->NotifyAddFriend(serverName, request);
    });
}

void ChatLogicSystem::friendAuthHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
    const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_FRIEND_AUTH_RSP));
    });
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);

    FriendApply applyInfo;
    applyInfo.fromJson(srcRoot);
    // 认证方发送的消息中，friend_id 是申请的发起人，uid 是认证方，所以更新申请时要调换 uid 和 friend_id
    std::swap(applyInfo.uid, applyInfo.friendId);
    if (applyInfo.friendId < 0 || applyInfo.uid < 0) {
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }

    if (!srcRoot.isMember("result")) {
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    if (const auto result = srcRoot["result"].asInt(); result != 1) {
        // 已拒绝好友，直接更新数据库删除缓存
        MysqlMgr::getInstance()->updateFriendApply(applyInfo.uid, applyInfo.friendId,
            static_cast<int>(FriendApplyStatus::REJECT));
        FriendCache::getInstance()->clearFriendApplyCount(applyInfo.friendId);
        return;
    }

    // 更新好友申请状态，并同步创建双向好友关系
    if (!MysqlMgr::getInstance()->createFriendRelation(applyInfo)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::MYSQL_ERROR);
        return;
    }
    // 清除被申请人的未读计数
    FriendCache::getInstance()->clearFriendApplyCount(applyInfo.friendId);

    // 推送好友请求信息
    notifyOnlineUserMsg(applyInfo.uid, data, MessageID::ID_NOTIFY_FRIEND_AUTH,
        [&applyInfo, &data, &root](const std::string& serverName) {
        ChatServiceReq request;
        request.set_from_uid(applyInfo.friendId);
        request.set_to_uid(applyInfo.uid);
        request.set_json(data);
        const auto resp = ChatGrpcClient::getInstance()->NotifyAuthFriend(serverName, request);
        root["error"] = resp.error();
    });
}

void ChatLogicSystem::updateFriendHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
    const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_UPDATE_FRIEND_RSP));
    });
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);

    FriendInfo info;
    info.fromJson(srcRoot);
    const auto uid = std::stoi(srcRoot["uid"].asString());
    if (!MysqlMgr::getInstance()->updateFriendRelation(uid, info)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::MYSQL_ERROR);
    }

    if (info.status == static_cast<int8_t>(FriendStatus::FRIEND_DELETED)) {
        // 单方面删除好友关系
        FriendCache::getInstance()->deleteFriendSet(uid, info.friendId);
    }
}

void ChatLogicSystem::updateUserInfoHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
                                           const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_UPDATE_USERINFO_RSP));
    });
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);

    // 暂时只能支持修改一个字段
    const auto uid = std::stoi(srcRoot["uid"].asString());
    UserBaseInfo oldInfo;
    oldInfo.uid = uid;
    if (!searchUserBaseInfo(oldInfo)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::USER_NOT_EXISTS);
        return;
    }

    UserBaseInfo info;
    info.fromJson(srcRoot);
    UserProfile profile;
    profile.fromJson(srcRoot);
    if (!MysqlMgr::getInstance()->updateUserBaseInfo(info)
        && !MysqlMgr::getInstance()->updateUserProfileInfo(profile)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::MYSQL_ERROR);
        return;
    }

    if (!RedisMgr::getInstance()->del(USER_BASE_INFO_PREFIX + std::to_string(uid))
        || !RedisMgr::getInstance()->del(USER_PROFILE_INFO_PREFIX + std::to_string(uid))
        || !RedisMgr::getInstance()->del(UID_INDEX_MAP_PREFIX + oldInfo.email.value())
        || !RedisMgr::getInstance()->del(UID_INDEX_MAP_PREFIX + oldInfo.name.value())) {
        std::cout << "Failed to delete user info, waiting to add delay task" << std::endl;
    }
}

// todo 后续优化性能
bool ChatLogicSystem::isPrivateChat(const int uid) {
    UserProfile profile;
    if (UserInfoCache::getUserProfile(uid, profile) && profile.privacyChat >= 0) {
        return profile.privacyChat == 1;
    }

    if (!MysqlMgr::getInstance()->selectUserProfileInfo(uid, profile)) {
        return false;
    }

    return profile.privacyChat == 1;
}

bool ChatLogicSystem::checkConversationValid(const int uid, const int other) {
    // 自己也可以跟自己建立会话
    if (uid == other) {
        return true;
    }

    if (isFriend(uid, other) || isPrivateChat(other)) {
        return true;
    }

    return false;
}

void ChatLogicSystem::conversationCreateHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
                                               const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_CHAT_CONVERSATION_RSP));
    });
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }

    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);

    ConversationInfo convInfo;
    convInfo.fromJson(srcRoot);
    if (!checkConversationValid(convInfo.uid, convInfo.friendId)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::CONV_CREATE_NO_PERMISSION);
        return;
    }

    // 创建单聊会话生成会话 ID
    convInfo.convType = static_cast<int8_t>(ConvType::PRIVATE_CHAT);
    convInfo.generateConvId();

    std::string result;
    if (!MysqlMgr::getInstance()->createConversation(convInfo, result)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::MYSQL_ERROR);
        return;
    }

    // 入库成功后生成回复消息内容
    convInfo.status = 0;
    convInfo.isTop = 0;
    convInfo.isMute = 0;
    convInfo.lastMsgContent = "";
    convInfo.lastTime = "";
    convInfo.updateTime = result;
    convInfo.toJson(root);
}

void ChatLogicSystem::getConversationTitleInfo(const ConversationInfo& convInfo, Json::Value &root) {
    const auto otherUid = convInfo.getOtherUid();
    if (otherUid < 0) {
        std::cout << "getConversationTitleInfo get other uid error" << std::endl;
        return;
    }
    UserBaseInfo userBaseInfo;
    userBaseInfo.uid = otherUid;
    if (!searchUserBaseInfo(userBaseInfo)) {
        return;
    }
    if (userBaseInfo.name.has_value()) {
        root["title"] = userBaseInfo.name.value();
    }
    if (userBaseInfo.avatarUrl.has_value()) {
        root["avatar_url"] = userBaseInfo.avatarUrl.value();
    }
}

void ChatLogicSystem::conversationListFetchHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
                                                  const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_CONV_LIST_RSP));
    });
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
    const auto uid = std::stoi(srcRoot["uid"].asString());
    auto sinceTime = srcRoot["since_update_time"].asString();
    if (sinceTime.empty()) {
        sinceTime = "0000-00-00 00:00:00";
    }
    const std::vector<ConversationInfo> searchResult = MysqlMgr::getInstance()->selectConversationList(uid, sinceTime);
    if (searchResult.empty()) {
        root["error"] = static_cast<int32_t>(ErrorCodes::FRIEND_APPLY_NOT_EXISTS);
        return;
    }

    for (auto& searchInfo : searchResult) {
        Json::Value info;
        searchInfo.toJson(info);
        searchInfo.uid = uid;
        getConversationTitleInfo(searchInfo, info);
        root["data"].append(info);
    }
}

void ChatLogicSystem::chatMsgHandle(const std::shared_ptr<Session> &session, uint16_t msgId, const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        Json::Value err;
        err["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        session->asyncSend(err.toStyledString(), static_cast<uint16_t>(MessageID::ID_CHAT_MSG_RSP));
        return;
    }
    Defer defer([&root, &session]() {
        session->asyncSend(root.toStyledString(), static_cast<uint16_t>(MessageID::ID_CONV_LIST_RSP));
    });

    // 解析消息
    MessageInfo info;
    info.fromJson(srcRoot);
    info.status = static_cast<uint8_t>(MessageStatus::SENDING);

    // 推入批量写入队列
    size_t shard_idx = std::hash<std::string>{}(session->getSessionId()) % shards_.size();
    auto node = std::make_shared<ChatMsgNode>(info, session);
    batch_writer_->bufferAt(shard_idx)->push(std::move(node));

    // 立即返回成功确认 (不含 serverId)
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
    root["msg_id"] = info.msgId;
    root["conv_id"] = info.convId.value_or("");

    // 通知接收方 (不依赖 serverId)
    notifyOnlineUserMsg(info.toUid, data, MessageID::ID_NOTIFY_CHAT_MSG,
        [&info, data](const std::string& serverName) {
            ChatServiceReq request;
            request.set_from_uid(info.fromUid);
            request.set_to_uid(info.toUid);
            request.set_json(data);
            ChatGrpcClient::getInstance()->SendChatMsg(serverName, request);
        });
}


void ChatLogicSystem::historyChatMsgFetchHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
    const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_CONV_HISTORY_MSG_RSP));
    });
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);

    const auto convId = srcRoot["conv_id"].asString();
    const auto sinceMsgId = srcRoot["since_msg_id"].asInt();
    const auto limit = srcRoot["limit"].asInt();
    const std::vector<MessageInfo> searchResult = MysqlMgr::getInstance()->selectMessageList(convId, sinceMsgId, limit);
    if (searchResult.empty()) {
        root["has_more"] = 0;
        return;
    }

    for (auto& searchInfo : searchResult) {
        Json::Value info;
        searchInfo.toJson(info);
        root["data"].append(info);
    }

    root["has_more"] = searchResult.size() < limit ? 0 : 1;
}

void ChatLogicSystem::msgStatusUpdateHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
    const std::string &data) {
    Json::Value root;
    Json::Value srcRoot;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_CONV_MSG_UPDATE_STATUS_RSP));
    });
    if (Json::Reader reader; !reader.parse(data, srcRoot)) {
        std::cout << "Failed to parse JSON data" << std::endl;
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        return;
    }
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);

    MessageStatusInfo info;
    info.fromJson(srcRoot);
    if (!MysqlMgr::getInstance()->updateConvMessagesStatus(info)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::MYSQL_ERROR);
        return;
    }
}

void ChatLogicSystem::heartbeatHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
                                      const std::string &data) {
    Json::Value root;
    Defer defer([&root, session]() {
        const std::string jsonStr = root.toStyledString();
        session->asyncSend(jsonStr, static_cast<uint16_t>(MessageID::ID_HEART_BEAT_RSP));
    });

    session->updateLstActiveTime();
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
}

/**
 * @brief 上传文件
 *
 * @note 再工作线程中处理，避免阻塞
 */
void ChatLogicSystem::uploadFileHandle(const std::shared_ptr<Session> &session, uint16_t msgId,
    const std::string &data) {
    // 发给工作线程处理，不要占用 IO 线程
    const auto worker = std::make_shared<LogicWorker>(session, msgId, data);
    worker->init();
    workerPool_.addTask(worker);
}
