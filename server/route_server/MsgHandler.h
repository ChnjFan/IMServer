//
// Created by fan on 24-11-20.
//

#ifndef IMSERVER_MSGHANDLER_H
#define IMSERVER_MSGHANDLER_H

#include <memory>
#include <map>
#include <functional>
#include "IMPdu.h"
#include "RouteConn.h"

class MsgHandlerCallbackMap {
public:
    using MsgHandlerCallback = std::function<void(RouteConn &conn, IMPdu&)>;

    MsgHandlerCallbackMap(const MsgHandlerCallbackMap&) = delete;

    static MsgHandlerCallbackMap* getInstance();
    static void destroyInstance();
    void registerHandler();
    void invokeCallback(uint32_t msgType, RouteConn &conn, IMPdu &imPdu);

private:
    MsgHandlerCallbackMap() = default;

    void registerCallback(uint32_t msgType, MsgHandlerCallback callback);

    static void handleHeartBeatMsg(RouteConn &conn, IMPdu &imPdu);
    static void handleLoginMsg(RouteConn &conn, IMPdu &imPdu);


    static MsgHandlerCallbackMap *instance;
    std::map<uint32_t, MsgHandlerCallback> callbackMap;
};

/**
 * @brief Route消息处理
 */
class MsgHandler {
public:
    explicit MsgHandler(RouteConn &conn, std::shared_ptr<IMPdu> &pImPdu);

    void exec();

private:
    RouteConn &conn;
    std::shared_ptr<IMPdu> &pImPdu;
};


#endif //IMSERVER_MSGHANDLER_H
