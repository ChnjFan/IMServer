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

void MsgHandlerCallbackMap::invokeCallback(std::string msgType, std::string connName, Base::Message& message) {
    auto it = callbackMap.find(msgType);
    if (it == callbackMap.end()) {// 无效消息直接丢弃
        return;
    }
    it->second(connName, message);
}

void MsgHandlerCallbackMap::registerCallback(const char *typeName, MsgHandlerCallbackMap::MsgHandlerCallback callback) {
    callbackMap[typeName] = std::move(callback);
}

void MsgHandlerCallbackMap::handleHeartBeatMsg(std::string connName, Base::Message& message) {
    //收到客户端心跳消息后要回复，并记录收到心跳的 time_tick
    std::cout << "Session " << connName << " recv heart beat" << std::endl;
}

void MsgHandlerCallbackMap::handleLoginMsg(std::string connName, Base::Message& message) {
    //登录消息检查会话状态，如果会话已经登录或正在验证，其他终端无法登录
}

void MsgDispatcher::init() {
    MsgHandlerCallbackMap::getInstance()->registerHandler();
}

void MsgDispatcher::exec(std::string connName, Base::MessagePtr &pMessage) {
    return MsgHandlerCallbackMap::getInstance()->invokeCallback(pMessage->getTypeName(), connName, *pMessage);
}

