//
// Created by Fan on 2026/5/12.
//

#ifndef IMSERVER_CHATLOGICSYSTEM_H
#define IMSERVER_CHATLOGICSYSTEM_H

#include <thread>
#include <atomic>
#include <functional>
#include <string>
#include <queue>

#include <json/json.h>

#include "const.h"
#include "FriendRelation.h"
#include "Singleton.h"
#include "MsgNode.h"
#include "ThreadPool.h"
#include "UserInfo.h"

struct ApplyUserInfo {
    uint8_t status;
    uint8_t pad[3];
    int uid;
    std::string name;
    std::string email;
};

struct ConversationInfo {
    std::string conv_id;
    uint8_t conv_type;
    uint8_t is_top;
    uint8_t is_mute;
    uint8_t pad;
    int to_uid;
    int unread_count;
    int last_msg_id;
    std::string last_msg;
    std::string last_time;
};

struct MessageInfo {
    std::string conv_id;
    int sender_uid;
    int receiver_uid;
    int msg_id;
    uint8_t msg_type;
    uint8_t status;
    uint8_t pad[2];
    std::string content;
};

typedef std::function<void(std::shared_ptr<Session> session, const uint16_t msgId, const std::string& data)> msgHandler;
typedef std::function<void(const std::string& serverName)> notifyDiffServerOnlineUserCallback;
typedef std::vector<std::shared_ptr<ApplyUserInfo>> ApplyUserList;
typedef std::vector<std::shared_ptr<ConversationInfo>> ConversationList;

class ChatLogicSystem : public Singleton<ChatLogicSystem> {
public:
    ~ChatLogicSystem();
    void close();

    void setServerName(const std::string& name);

    void insertMsgNode(const std::shared_ptr<LogicNode> &msg);

    void notifyOnlineUserMsg(int uid, const std::string& msg, MessageID msgId, const notifyDiffServerOnlineUserCallback &callback);

    static bool searchUserInfoByUid(int uid, UserInfo& userInfo);

private:
    friend class Singleton<ChatLogicSystem>;

    // 初始化聊天服务逻辑
    ChatLogicSystem();
    void initHandlers();
    void registerHandler(uint16_t msgId, const msgHandler& handler);
    // 处理消息
    void dealMsg();
    void handleMsgNode(const std::shared_ptr<LogicNode>& node);

    // todo 待修复
    bool getConversationList(int uid, ConversationList& convList);
    void addHistoryMessage(Json::Value& root, ChatMsgStatus status);

    // 客户端踢人逻辑
    void kickOnlineUser(int uid) const;

    // 登录逻辑
    void loginHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);
    int getApplyFriendCount(int uid);
    void firstPageInfoHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);

    bool getFriendList(int uid, int sinceId, std::vector<FriendInfo>& friendList);
    void searchFriendListHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);

    // 搜索好友用户
    static void getSearchInfoFromJson(Json::Value& root, UserInfo& userInfo);
    static std::string getSearchKey(UserInfo& userInfo);
    static bool searchUserInfo(UserInfo& userInfo);
    void searchUserHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);

    // 查询用户详细信息
    static bool searchUserFullInfoInRedis(UserFullInfo& userFullInfo);
    static bool searchUserFullInfoByUid(int uid, UserFullInfo& userFullInfo);

    // 添加好友
    bool getFriendRelationFromRedis(int from, int uid, FriendRelation& fr);
    bool getFriendRelation(int from, int uid, FriendRelation& fr);
    void setFriendRelation(int from, int uid, Json::Value& root);
    void searchUserFullInfoHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);

    bool getFriendApplyList(int uid, int sinceId, std::vector<FriendApplyInfo>& applyInfoList);
    void searchFriendApplyListHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);

    // 认证好友
    static bool checkFriendRelation(int uid, int friendId);
    static bool checkFriendApply(int uid, int friendId);
    static bool checkFriendApplyInvalid(int uid, int friendId);
    void friendApplyHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);
    void friendAuthHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);

    // 修改用户信息
    void updateUserInfoHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);

    void chatMsgHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);
    void conversationCreateHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);

    void uploadFileHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);

    void heartbeatHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);

    std::atomic<bool> stop_;
    std::thread worker_;

    std::string selfServerName_;

    std::queue<std::shared_ptr<LogicNode>> msgQueue_;
    std::mutex mutex_;
    std::condition_variable cond_;

    ThreadPool workerPool_;

    std::unordered_map<uint16_t, msgHandler> handlers_;
};


#endif //IMSERVER_CHATLOGICSYSTEM_H