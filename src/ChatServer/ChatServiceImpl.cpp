//
// Created by Fan on 2026/5/18.
//

#include "ChatServiceImpl.h"

ChatServiceImpl::ChatServiceImpl() {
}

Status ChatServiceImpl::NotifyAddFriend(ServerContext *context, const message::AddFriendReq *request,
    message::AddFriendRsp *response) {
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
    return Status::OK;
}

Status ChatServiceImpl::NotifyTextChatMsg(ServerContext *context, const message::TextChatData *request,
    message::TextChatMsgRsp *response) {
    return Status::OK;
}
