#ifndef IMSERVER_PERF_USER_SETUP_H
#define IMSERVER_PERF_USER_SETUP_H

#include "chat_test_client.h"
#include "grpc_test_client.h"
#include "message.grpc.pb.h"
#include "account_manager.h"
#include "ConfigMgr.h"

using message::GetChatServerReq;
using message::GetChatServerRsp;
using message::StatusService;

// 压力测试用户注册 + 获取真实 token 的完整流程
// 返回有效的 token（从 StatusServer 获取），供 chatLogin 使用
inline std::string acquireTestToken(int uid, const std::string& tag = "perf") {
    // 1. 通过 GateServer 注册内部测试用户（写验证码到 Redis + 落库 MySQL）
    AccountManager mgr;
    auto acct = mgr.acquire(tag + "_" + std::to_string(uid));

    // 2. 从 StatusServer 获取真实有效的 token
    auto& config = ConfigMgr::getInstance();
    GrpcTestClient<StatusService> statusCli(
        "127.0.0.1",
        static_cast<uint16_t>(std::stoi(config["StatusServer"]["Port"])));

    GetChatServerReq req;
    req.set_uid(acct.uid);
    GetChatServerRsp rsp;
    grpc::ClientContext ctx;
    auto status = statusCli.stub()->GetChatServer(&ctx, req, &rsp);
    if (!status.ok() || rsp.error() != 0) {
        throw std::runtime_error("Failed to get token for uid=" +
                                 std::to_string(acct.uid));
    }
    return rsp.token();
}

// 带真实 token 的登录（注册 + 取 token + 登录一步完成）
inline bool loginWithRealToken(ChatTestClient& client, int uid,
                               const std::string& tag = "perf") {
    std::string token = acquireTestToken(uid, tag);
    return client.chatLogin(uid, token);
}

#endif
