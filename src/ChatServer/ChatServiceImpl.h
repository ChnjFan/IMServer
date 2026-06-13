//
// Created by Fan on 2026/5/18.
//

#ifndef IMSERVER_CHATSERVICEIMPL_H
#define IMSERVER_CHATSERVICEIMPL_H

#include <grpcpp/grpcpp.h>

#include "message.pb.h"
#include "message.grpc.pb.h"

using grpc::ServerContext;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerUnaryReactor;
using grpc::Status;

using message::AddFriendReq;
using message::AddFriendRsp;
using message::AuthFriendReq;
using message::AuthFriendRsp;
using message::ChatService;

class ChatServiceImpl final : public ChatService::Service {
public:
    ChatServiceImpl();

    Status NotifyAddFriend(ServerContext* context, const ::message::AddFriendReq* request, ::message::AddFriendRsp* response) override;
    Status ReplyAddFriend(ServerContext* context, const ::message::ReplyFriendReq* request, ::message::ReplyFriendRsp* response) override;
    Status SendChatMsg(ServerContext* context, const ::message::SendChatMsgReq* request, ::message::SendChatMsgRsp* response) override;
    Status NotifyAuthFriend(ServerContext* context, const ::message::AuthFriendReq* request, ::message::AuthFriendRsp* response) override;
    Status NotifyTextChatMsg(ServerContext* context, const ::message::TextChatData* request, ::message::TextChatMsgRsp* response) override;
    Status NotifyOffline(grpc::ServerContext *context, const message::UserOfflineReq *request, message::UserOfflineRsp *response) override;
};


#endif //IMSERVER_CHATSERVICEIMPL_H