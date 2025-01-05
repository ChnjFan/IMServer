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

void MsgHandlerCallbackMap::invokeCallback(std::string msgType, ServerNet::ServiceHandler *pClient, Base::Message& message) {
    auto it = callbackMap.find(msgType);
    if (it == callbackMap.end()) {// 无效消息直接丢弃
        return;
    }
    it->second(pClient, message);
}

void MsgHandlerCallbackMap::registerCallback(const char *typeName, MsgHandlerCallbackMap::MsgHandlerCallback callback) {
    callbackMap[typeName] = std::move(callback);
}

void MsgHandlerCallbackMap::handleHeartBeatMsg(ServerNet::ServiceHandler *pClient, Base::Message& message) {
    //收到客户端心跳消息后要回复，并记录收到心跳的 time_tick
    std::cout << "Session "  << " recv heart beat" << std::endl;
}

void MsgHandlerCallbackMap::handleLoginMsg(ServerNet::ServiceHandler *pClient, Base::Message& message) {
    //登录消息检查会话状态，如果会话已经登录或正在验证，其他终端无法登录
}

void MsgDispatcher::init() {
    MsgHandlerCallbackMap::getInstance()->registerHandler();
}

void MsgDispatcher::exec(ServerNet::ServiceHandler *pClient, Base::Message& message) {
    return MsgHandlerCallbackMap::getInstance()->invokeCallback(message.getTypeName(), pClient, message);
}

void MsgDispatcher::request(ServerNet::ServiceHandler *pClient, Base::Message& taskMessage) {
    exec(pClient, taskMessage);
}

