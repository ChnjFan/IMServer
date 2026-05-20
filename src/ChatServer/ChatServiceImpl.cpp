//
// Created by Fan on 2026/5/18.
//

#include "ChatServiceImpl.h"

#include <json/value.h>

#include "UserMgr.h"
#include "const.h"
#include "Session.h"
#include "ChatLogicSystem.h"

ChatServiceImpl::ChatServiceImpl() {
}

Status ChatServiceImpl::NotifyAddFriend(ServerContext *context, const message::AddFriendReq *request,
                                        message::AddFriendRsp *response) {
    Defer defer([request, response]() {
        response->set_error(static_cast<int32_t>(ErrorCodes::SUCCESS));
        response->set_from_uid(request->from_uid());
        response->set_to_uid(request->to_uid());
    });
    // 校验用户是否在在线
    auto touid = request->to_uid();
    auto session = UserMgr::getInstance()->getSession(touid);
    if (nullptr == session) {
        return Status::OK;
    }

    Json::Value data;
    data["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
    data["fromUid"] = request->from_uid();
    data["applyName"] = request->name();
    data["applyEmail"] = request->email();
    session->asyncSend(data.toStyledString(), static_cast<uint16_t>(MessageID::ID_NOTIFY_FRIEND_ADD));

    return Status::OK;
}

Status ChatServiceImpl::ReplyAddFriend(ServerContext *context, const message::ReplyFriendReq *request,
    message::ReplyFriendRsp *response) {
    return Status::OK;
}

Status ChatServiceImpl::SendChatMsg(ServerContext *context, const message::SendChatMsgReq *request,
    message::SendChatMsgRsp *response) {
    return Status::OK;
}

Status ChatServiceImpl::NotifyAuthFriend(ServerContext *context, const message::AuthFriendReq *request,
    message::AuthFriendRsp *response) {
    Defer defer([request, response]() {
        response->set_error(static_cast<int32_t>(ErrorCodes::SUCCESS));
        response->set_from_uid(request->from_uid());
        response->set_to_uid(request->to_uid());
    });
    // 校验用户是否在在线
    const auto toUid = request->to_uid();
    const auto session = UserMgr::getInstance()->getSession(toUid);
    if (nullptr == session) {
        return Status::OK;
    }

    // 获取用户信息
    const auto fromUid = request->from_uid();
    auto userInfo = std::make_shared<UserInfo>();
    if (!ChatLogicSystem::getUserBaseInfo(USER_BASE_INFO_PREFIX + std::to_string(fromUid), fromUid, userInfo)) {
        return Status::OK;
    }

    Json::Value data;
    data["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
    data["from_uid"] = fromUid;
    data["to_uid"] = toUid;
    data["name"] = userInfo->name;
    data["email"] = userInfo->email;
    session->asyncSend(data.toStyledString(), static_cast<uint16_t>(MessageID::ID_NOTIFY_FRIEND_AUTH));

    return Status::OK;
}

Status ChatServiceImpl::NotifyTextChatMsg(ServerContext *context, const message::TextChatData *request,
    message::TextChatMsgRsp *response) {
    return Status::OK;
}
