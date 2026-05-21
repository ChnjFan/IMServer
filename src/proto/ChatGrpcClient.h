//
// Created by Fan on 2026/5/18.
//

#ifndef IMSERVER_CHATGRPCCLIENT_H
#define IMSERVER_CHATGRPCCLIENT_H

#include <memory>
#include <grpcpp/grpcpp.h>

#include "message.pb.h"
#include "message.grpc.pb.h"

#include "Singleton.h"
#include "ServiceConnPool.h"

using grpc::Channel;
using grpc::Status;
using grpc::ClientContext;

using message::AddFriendReq;
using message::AddFriendRsp;
using message::AuthFriendReq;
using message::AuthFriendRsp;
using message::ChatService;

class ChatGrpcClient : public Singleton<ChatGrpcClient> {
public:
    typedef std::unique_ptr<ServiceConnPool<ChatService>> ChatServicePool;

    AddFriendRsp NotifyAddFriend(std::string& serviceName, const AddFriendReq &request);

    AuthFriendRsp NotifyAuthFriend(std::string& serviceName, const AuthFriendReq &request);

private:
    friend class Singleton<ChatGrpcClient>;

    ChatGrpcClient();

    std::unordered_map<std::string, ChatServicePool> pools_;
};


#endif //IMSERVER_CHATGRPCCLIENT_H