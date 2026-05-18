//
// Created by Fan on 2026/5/12.
//

#ifndef IMSERVER_CHATLOGICSYSTEM_H
#define IMSERVER_CHATLOGICSYSTEM_H

#include <thread>
#include <atomic>
#include <functional>
#include <string>

#include "const.h"
#include "Singleton.h"
#include "MsgNode.h"

typedef std::function<void(std::shared_ptr<Session> session, const uint16_t msgId, const std::string& data)> msgHandler;
class ChatLogicSystem : public Singleton<ChatLogicSystem> {
public:
    ~ChatLogicSystem();
    void close();

    void insertMsgNode(const std::shared_ptr<LogicNode> &msg);
private:
    friend class Singleton<ChatLogicSystem>;

    ChatLogicSystem();
    void initHandlers();
    void registerHandler(uint16_t msgId, const msgHandler& handler);

    void dealMsg();
    void handleMsgNode(const std::shared_ptr<LogicNode>& node);

    bool getUserBaseInfo(const std::string &key, int uid, std::shared_ptr<UserInfo>& userInfo);

    void loginHandle(const std::shared_ptr<Session>& session, uint16_t msgId, const std::string& data);

    std::atomic<bool> stop_;
    std::thread worker_;

    std::queue<std::shared_ptr<LogicNode>> msgQueue_;
    std::mutex mutex_;
    std::condition_variable cond_;

    std::unordered_map<uint16_t, msgHandler> handlers_;
};


#endif //IMSERVER_CHATLOGICSYSTEM_H