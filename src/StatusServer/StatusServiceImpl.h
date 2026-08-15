//
// Created by Fan on 2026/5/10.
//

#ifndef IMSERVER_STATUSSERVICEIMPL_H
#define IMSERVER_STATUSSERVICEIMPL_H

#include <unordered_map>
#include <mutex>

#include "message.pb.h"
#include "message.grpc.pb.h"

#include "ConfigMgr.h"

using grpc::ServerContext;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerUnaryReactor;
using grpc::Status;

using message::GetResourceServerReq;
using message::GetResourceServerRsp;
using message::GetChatServerReq;
using message::GetChatServerRsp;
using message::StatusService;
using message::VerifyTokenReq;
using message::VerifyTokenRsp;

struct ServerInfo {
    std::string host;
    std::string port;
    std::string httpPort;
    std::string name;
    int connCount;          // 连接计数
};

class StatusServiceImpl final : public StatusService::Service {
public:
    StatusServiceImpl();
    Status GetResourceServer(ServerContext *context, const GetResourceServerReq *request, GetResourceServerRsp *response) override;
    Status GetChatServer(ServerContext* context, const GetChatServerReq* request, GetChatServerRsp* response) override;
    Status Login(ServerContext *context, const message::LoginReq *request, message::LoginRsp *response) override;
    Status VerifyToken(ServerContext* context, const VerifyTokenReq* request, VerifyTokenRsp* response) override;

private:
    void initChatServer(ConfigMgr& config);
    void initResourceServer(ConfigMgr& config);

    ServerInfo getChatServerInfo();

    void insertToken(int uid, const std::string& token);
    bool checkToken(int uid, const std::string& token);

    std::unordered_map<std::string, ServerInfo> chatServers_;
    std::unordered_map<std::string, ServerInfo> resourceServers_;
    std::mutex serverMutex_;
};


#endif //IMSERVER_STATUSSERVICEIMPL_H