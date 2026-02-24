#pragma once

#ifndef USER_SERVICE_IMPL_H
#define USER_SERVICE_IMPL_H

#include <grpcpp/grpcpp.h>
#include <unordered_map>
#include <string>
#include <ctime>

#include "service_user.grpc.pb.h"
#include "common_messages.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using im::common::protocol::UserService;
using im::common::protocol::LoginRequest;
using im::common::protocol::LoginResponse;
using im::common::protocol::StatusResponse;
using google::protobuf::Empty;

class UserServiceImpl final : public UserService::Service {
private:
    std::unordered_map<std::string, std::string> userDatabase;  // 模拟用户数据库
    time_t start_time_;

public:
    UserServiceImpl();
    ~UserServiceImpl() = default;

    // 实现Login方法
    Status Login(ServerContext* context, const LoginRequest* request, LoginResponse* response) override;

    // 实现CheckStatus方法
    Status CheckStatus(ServerContext* context, const Empty* request, StatusResponse* response) override;

private:
    // 验证用户凭据
    bool validateUser(const std::string& username, const std::string& password);

    // 生成token
    std::string generateToken(const std::string& username);

    // 获取用户ID
    std::string getUserID(const std::string& username);

    // 检查服务健康状态
    bool checkServiceHealth();
};

#endif // USER_SERVICE_IMPL_H
