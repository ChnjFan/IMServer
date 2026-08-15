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
#include "const.h"

using grpc::Channel;
using grpc::Status;
using grpc::ClientContext;

using message::ChatService;
using message::ChatServiceReq;
using message::ChatServiceRsp;

class ChatGrpcClient : public Singleton<ChatGrpcClient> {
public:
    typedef std::unique_ptr<ServiceConnPool<ChatService>> ChatServicePool;

    ChatServiceRsp NotifyAddFriend(const std::string& serviceName, const ChatServiceReq &request);

    ChatServiceRsp NotifyAuthFriend(const std::string& serviceName, const ChatServiceReq &request);

    ChatServiceRsp SendChatMsg(const std::string& serviceName, const ChatServiceReq& request);

    ErrorCodes NotifyOffline(const std::string& serviceName, int uid);

private:
    friend class Singleton<ChatGrpcClient>;

    ChatGrpcClient();

    std::unordered_map<std::string, ChatServicePool> pools_;
};


#endif //IMSERVER_CHATGRPCCLIENT_H