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

using message::ChatService;
using message::ChatServiceReq;
using message::ChatServiceRsp;

class ChatServiceImpl final : public ChatService::Service {
public:
    ChatServiceImpl();

    Status NotifyAddFriend(ServerContext* context, const ChatServiceReq* request, ChatServiceRsp* response) override;
    Status ReplyAddFriend(ServerContext* context, const ChatServiceReq* request, ChatServiceRsp* response) override;
    Status NotifyAuthFriend(ServerContext* context, const ChatServiceReq* request, ChatServiceRsp* response) override;
    Status SendChatMsg(ServerContext* context, const ChatServiceReq* request, ChatServiceRsp* response) override;
    Status NotifyOffline(ServerContext* context, const ChatServiceReq* request, ChatServiceRsp* response) override;
};


#endif //IMSERVER_CHATSERVICEIMPL_H