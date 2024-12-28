//
// Created by fan on 24-11-20.
//

#include <utility>
#include "IM.BaseType.pb.h"
#include "MsgDispatcher.h"

MsgHandlerCallbackMap *MsgHandlerCallbackMap::instance = nullptr;

MsgHandlerCallbackMap *MsgHandlerCallbackMap::getInstance() {
    if (instance == nullptr)
        instance = new MsgHandlerCallbackMap();
    return instance;
}

void MsgHandlerCallbackMap::destroyInstance() {
    delete instance;
    instance = nullptr;
}

void MsgHandlerCallbackMap::registerHandler() {
    registerCallback("IM.BaseType.ImMsgHeartBeat", MsgHandlerCallbackMap::handleHeartBeatMsg);
    registerCallback("IM.Account.ImMsgLoginReq", MsgHandlerCallbackMap::handleLoginMsg);
}

void MsgHandlerCallbackMap::invokeCallback(std::string msgType, SessionConn &conn, Base::Message& message) {
    auto it = callbackMap.find(msgType);
    if (it == callbackMap.end()) {// 无效消息直接丢弃
        return;
    }
    it->second(conn, message);
}

void MsgHandlerCallbackMap::registerCallback(const char *typeName, MsgHandlerCallbackMap::MsgHandlerCallback callback) {
    callbackMap[typeName] = std::move(callback);
}

void MsgHandlerCallbackMap::handleHeartBeatMsg(SessionConn &conn, Base::Message& message) {
    //收到客户端心跳消息后要回复，并记录收到心跳的 time_tick
    std::cout << "Session " << conn.getSessionUID() << " recv heart beat" << std::endl;
    conn.sendMsg(message);
    conn.updateTimeStamp();
}

void MsgHandlerCallbackMap::handleLoginMsg(SessionConn &conn, Base::Message& message) {
    //登录消息检查会话状态，如果会话已经登录或正在验证，其他终端无法登录
    if (!conn.isConnIdle())
        return;


    conn.setState(ROUTE_CONN_STATE::CONN_VERIFY);
}

void MsgDispatcher::exec(SessionConn &conn, Base::MessagePtr &pMessage) {
    return MsgHandlerCallbackMap::getInstance()->invokeCallback(pMessage->getTypeName(), conn, *pMessage);
}

