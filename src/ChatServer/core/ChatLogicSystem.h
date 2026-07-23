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
#include <boost/lockfree/queue.hpp>

#include <json/json.h>

#include "const.h"
#include "Singleton.h"
#include "MsgNode.h"
#include "MysqlMgr.h"
#include "ThreadPool.h"
#include "common/model/UserBaseInfo.h"

typedef std::function<void(std::shared_ptr<Session> session, const uint16_t msgId, const std::string& data)> msgHandler;
typedef std::function<void(const std::string& serviceName)> notifyOnlineUserCallback;

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
    void initHandlers();
    void registerHandler(uint16_t msgId, const msgHandler& handler);
    // 处理消息
    void dealMsg();
    void handleMsgNode(const std::shared_ptr<LogicNode>& node);

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
    static bool checkFriendRelation(int uid, int friendId);
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

    // 无锁队列：IO 线程 push、worker 线程 pop 都不需要抢锁
    // 存储堆分配的 shared_ptr 的原始指针（原始指针 trivially destructible）
    // 堆分配的 shared_ptr 保证对象在队列中始终存活
    boost::lockfree::queue<std::shared_ptr<LogicNode>*> msgQueue_;
    // mutex + cond_ 仅用于 worker 线程在无消息时休眠（避免空转 CPU）
    std::mutex mutex_;
    std::condition_variable cond_;

    // 性能统计
    mutable std::mutex stats_mutex_;
    uint64_t total_queue_wait_us_ = 0;   // 消息在队列中等待的总时间
    uint64_t total_process_us_ = 0;      // 业务处理的总时间
    uint64_t total_messages_ = 0;        // 处理的消息总数
    std::chrono::steady_clock::time_point last_report_time_;

    ThreadPool workerPool_;

    std::unordered_map<uint16_t, msgHandler> handlers_;
};


#endif //IMSERVER_CHATLOGICSYSTEM_H