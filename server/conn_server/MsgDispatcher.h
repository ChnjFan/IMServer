//
// Created by fan on 24-11-20.
//

#ifndef IMSERVER_MSGDISPATCHER_H
#define IMSERVER_MSGDISPATCHER_H

#include <map>
#include <string>
#include <memory>
#include <functional>
#include "Message.h"
#include "ServiceWorker.h"

/**
* @class MsgHandlerCallbackMap
* @brief 消息分发器。注册消息回调并发送到各业务，异常消息处理。
*/
class MsgHandlerCallbackMap {
public:
    using MsgHandlerCallback = std::function<void(ServerNet::ServiceHandler *pClient, Base::Message&)>;

    MsgHandlerCallbackMap(const MsgHandlerCallbackMap&) = delete;

    static MsgHandlerCallbackMap* getInstance();
    static void destroyInstance();
    void registerHandler();
    void invokeCallback(std::string msgType, ServerNet::ServiceHandler *pClient, Base::Message &message);

private:
    MsgHandlerCallbackMap() = default;

    void registerCallback(const char *typeName, MsgHandlerCallback callback);

    static void handleHeartBeatMsg(ServerNet::ServiceHandler *pClient, Base::Message &message);
    static void handleLoginMsg(ServerNet::ServiceHandler *pClient, Base::Message &message);


    static MsgHandlerCallbackMap *instance;
    std::map<std::string, MsgHandlerCallback> callbackMap;
};

/**
* @class MsgDispatcher
* @brief 消息分发
*/
class MsgDispatcher : public ServerNet::ServiceWorker {
public:
    explicit MsgDispatcher(ServerNet::ServiceProvider* server) : ServerNet::ServiceWorker(server) { }
    static void init();
    static void exec(ServerNet::ServiceHandler *pClient, Base::Message& message);

    void request(ServerNet::ServiceHandler* pClient, Base::Message& taskMessage) override;

};


#endif //IMSERVER_MSGDISPATCHER_H
