//
// Created by Fan on 2026/5/10.
//

#ifndef IMSERVER_STATUSSERVICEIMPL_H
#define IMSERVER_STATUSSERVICEIMPL_H

#include <unordered_map>
#include <mutex>

#include "message.pb.h"
#include "message.grpc.pb.h"


using grpc::ServerContext;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerUnaryReactor;
using grpc::Status;

using message::GetChatServerReq;
using message::GetChatServerRsp;
using message::StatusService;

struct ChatServerInfo {
    std::string host;
    std::string port;
    std::string name;
    int connCount;          // 连接计数
};

class StatusServiceImpl : public message::StatusService::Service {
public:
    StatusServiceImpl() = default;
    Status GetChatServer(ServerContext* context, const GetChatServerReq* request, GetChatServerRsp* response) override;

private:
    ChatServerInfo getChatServerInfo();
    void insertToken(int uid, std::string token);

    std::unordered_map<std::string, ChatServerInfo> chatServers_;
    std::unordered_map<int, std::string> tokens_;
    std::mutex serverMutex_;
    std::mutex tokenMutex_;
};


#endif //IMSERVER_STATUSSERVICEIMPL_H