//
// Created by fan on 24-11-20.
//

#ifndef IMSERVER_MSGDISPATCH_H
#define IMSERVER_MSGDISPATCH_H

#include <memory>
#include <map>
#include <functional>
#include "IMPdu.h"
#include "SessionConn.h"

class MsgHandlerCallbackMap {
public:
    using MsgHandlerCallback = std::function<void(SessionConn &conn, Common::IMPdu&)>;

    MsgHandlerCallbackMap(const MsgHandlerCallbackMap&) = delete;

    static MsgHandlerCallbackMap* getInstance();
    static void destroyInstance();
    void registerHandler();
    void invokeCallback(uint32_t msgType, SessionConn &conn, Common::IMPdu &imPdu);

private:
    MsgHandlerCallbackMap() = default;

    void registerCallback(uint32_t msgType, MsgHandlerCallback callback);

    static void handleHeartBeatMsg(SessionConn &conn, Common::IMPdu &imPdu);
    static void handleLoginMsg(SessionConn &conn, Common::IMPdu &imPdu);


    static MsgHandlerCallbackMap *instance;
    std::map<uint32_t, MsgHandlerCallback> callbackMap;
};

/**
 * @brief Route消息处理
 */
class MsgDispatch {
public:
    static void exec(SessionConn &conn, std::shared_ptr<Common::IMPdu> &pImPdu);

};


#endif //IMSERVER_MSGDISPATCH_H
