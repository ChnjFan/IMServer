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

class StatusServiceImpl final : public StatusService::Service {
public:
    StatusServiceImpl();
    Status GetChatServer(ServerContext* context, const GetChatServerReq* request, GetChatServerRsp* response) override;
    Status Login(ServerContext *context, const message::LoginReq *request, message::LoginRsp *response) override;

private:
    ChatServerInfo getChatServerInfo();

    void insertToken(int uid, const std::string& token);
    bool checkToken(int uid, const std::string& token);

    std::unordered_map<std::string, ChatServerInfo> chatServers_;
    std::mutex serverMutex_;
};


#endif //IMSERVER_STATUSSERVICEIMPL_H