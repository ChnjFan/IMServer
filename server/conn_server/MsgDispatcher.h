//
// Created by fan on 24-11-20.
//

#ifndef IMSERVER_MSGDISPATCHER_H
#define IMSERVER_MSGDISPATCHER_H

#include <memory>
#include <map>
#include <functional>
#include "Message.h"
#include "SessionConn.h"

class MsgHandlerCallbackMap {
public:
    using MsgHandlerCallback = std::function<void(SessionConn &conn, Base::Message&)>;

    MsgHandlerCallbackMap(const MsgHandlerCallbackMap&) = delete;

    static MsgHandlerCallbackMap* getInstance();
    static void destroyInstance();
    void registerHandler();
    void invokeCallback(uint32_t msgType, SessionConn &conn, Base::Message &message);

private:
    MsgHandlerCallbackMap() = default;

    void registerCallback(uint32_t msgType, MsgHandlerCallback callback);

    static void handleHeartBeatMsg(SessionConn &conn, Base::Message &message);
    static void handleLoginMsg(SessionConn &conn, Base::Message &message);


    static MsgHandlerCallbackMap *instance;
    std::map<uint32_t, MsgHandlerCallback> callbackMap;
};

/**
 * @brief Route消息处理
 */
class MsgDispatcher {
public:
    static void exec(SessionConn &conn, Base::MessagePtr &pMessage);
};


#endif //IMSERVER_MSGDISPATCHER_H
