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
#include "Singleton.h"
#include "MsgNode.h"
#include "ThreadPool.h"

struct ApplyUserInfo {
    uint8_t status;
    uint8_t pad[3];
    int uid;
    std::string name;
    std::string email;
};

struct FriendInfo {
    int uid;
    uint8_t status;
    uint8_t isStar;
    uint8_t isHidden;
    uint8_t pad;
    std::string name;
    std::string email;
    std::string alias;
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
typedef std::vector<std::shared_ptr<ApplyUserInfo>> ApplyUserList;
typedef std::vector<std::shared_ptr<FriendInfo>> FriendInfoList;
typedef std::vector<std::shared_ptr<ConversationInfo>> ConversationList;

class ChatLogicSystem : public Singleton<ChatLogicSystem> {
public:
    ~ChatLogicSystem();
    void close();

    void setServerName(const std::string& name);

    void insertMsgNode(const std::shared_ptr<LogicNode> &msg);

    static bool getUserBaseInfo(const std::string &key, int uid, std::shared_ptr<UserInfo>& userInfo);

private:
    friend class Singleton<ChatLogicSystem>;

    ChatLogicSystem();
    void initHandlers();
    void registerHandler(uint16_t msgId, const msgHandler& handler);

    void dealMsg();
    void handleMsgNode(const std::shared_ptr<LogicNode>& node);

    bool getUserInfoByName(const std::string &name, Json::Value& root);
    bool getConversationList(int uid, ConversationList& convList);

    void addHistoryMessage(Json::Value& root, ChatMsgStatus status);

    /**
     * 客户端踢人逻辑
     */
    void kickOnlineUser(int uid) const;

    void loginHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);
    void searchUserHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);
    void addFriendHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);
    void friendAuthHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);

    void chatMsgHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);
    void conversationCreateHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);

    void uploadFileHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);

    void heartbeatHanlde(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);

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