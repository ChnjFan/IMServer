//
// Created by fan on 24-11-20.
//

#include <utility>
#include "IM.BaseType.pb.h"
#include "MsgHandler.h"
#include "LoginServerResult.h"

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
    registerCallback(IM::BaseType::MSG_TYPE_HEARTBEAT, MsgHandlerCallbackMap::handleHeartBeatMsg);
    registerCallback(IM::BaseType::MSG_TYPE_LOGIN, MsgHandlerCallbackMap::handleLoginMsg);
}

void MsgHandlerCallbackMap::invokeCallback(uint32_t msgType, RouteConn &conn, Common::IMPdu &imPdu) {
    auto it = callbackMap.find(msgType);
    if (it == callbackMap.end()) {
        return;
    }
    it->second(conn, imPdu);
}

void MsgHandlerCallbackMap::registerCallback(uint32_t msgType, MsgHandlerCallbackMap::MsgHandlerCallback callback) {
    callbackMap[msgType] = std::move(callback);
}

void MsgHandlerCallbackMap::handleHeartBeatMsg(RouteConn &conn, Common::IMPdu &imPdu) {
    //收到客户端心跳消息后要回复，并记录收到心跳的 time_tick
    std::cout << "Session " << conn.getSessionUID() << " recv heart beat" << std::endl;
    conn.sendPdu(imPdu);
    conn.updateLsgTimeStamp();
}

void MsgHandlerCallbackMap::handleLoginMsg(RouteConn &conn, Common::IMPdu &imPdu) {
    //登录消息检查会话状态，如果会话已经登录或正在验证，其他终端无法登录
    if (!conn.isConnIdle())
        return;
    conn.setState(ROUTE_CONN_STATE::CONN_VERIFY);
    LoginServerResult::getInstance()->sendPdu(imPdu);
}


void MsgHandler::exec(RouteConn &conn, std::shared_ptr<Common::IMPdu> &pImPdu) {
    return MsgHandlerCallbackMap::getInstance()->invokeCallback(pImPdu->getMsgType(), conn, *pImPdu);
}

