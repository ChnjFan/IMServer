//
// Created by Fan on 2026/5/18.
//

#include "ChatServiceImpl.h"

#include <json/value.h>

#include "UserMgr.h"
#include "const.h"
#include "Session.h"
#include "ChatLogicSystem.h"
#include "RedisMgr.h"

ChatServiceImpl::ChatServiceImpl() {
}

Status ChatServiceImpl::NotifyAddFriend(ServerContext *context, const ChatServiceReq *request,
    ChatServiceRsp *response) {
    const auto to = request->to_uid();
    const auto session = UserMgr::getInstance()->getSession(to);
    if (!session) {
        response->set_error(static_cast<int32_t>(ErrorCodes::USER_IS_OFFLINE));
        return Status::OK;
    }
    session->asyncSend(request->json(), static_cast<uint16_t>(MessageID::ID_NOTIFY_FRIEND_APPLY));
    return Status::OK;
}

Status ChatServiceImpl::ReplyAddFriend(ServerContext *context, const ChatServiceReq *request,
    ChatServiceRsp *response) {
    return Status::OK;
}

Status ChatServiceImpl::NotifyAuthFriend(ServerContext *context, const ChatServiceReq *request,
    ChatServiceRsp *response) {
    const auto to = request->to_uid();
    const auto session = UserMgr::getInstance()->getSession(to);
    if (!session) {
        response->set_error(static_cast<int32_t>(ErrorCodes::USER_IS_OFFLINE));
        return Status::OK;
    }

    session->asyncSend(request->json(), static_cast<uint16_t>(MessageID::ID_NOTIFY_FRIEND_AUTH));
    return Status::OK;
}

Status ChatServiceImpl::SendChatMsg(ServerContext *context, const ChatServiceReq *request, ChatServiceRsp *response) {
    const auto to = request->to_uid();
    const auto session = UserMgr::getInstance()->getSession(to);
    if (!session) {
        response->set_error(static_cast<int32_t>(ErrorCodes::USER_IS_OFFLINE));
        return Status::OK;
    }

    session->asyncSend(request->json(), static_cast<uint16_t>(MessageID::ID_NOTIFY_CHAT_MSG));
    return Status::OK;
}

Status ChatServiceImpl::NotifyOffline(ServerContext *context, const ChatServiceReq *request, ChatServiceRsp *response) {
    const auto to = request->to_uid();
    const auto session = UserMgr::getInstance()->getSession(to);
    if (!session) {
        response->set_error(static_cast<int32_t>(ErrorCodes::USER_IS_OFFLINE));
        return Status::OK;
    }

    session->notifyOffline();
    return Status::OK;
}
