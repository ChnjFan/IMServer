#ifndef IMSERVER_PERF_USER_SETUP_H
#define IMSERVER_PERF_USER_SETUP_H

#include "chat_test_client.h"
#include "grpc_test_client.h"
#include "message.grpc.pb.h"
#include "account_manager.h"
#include "AsioIOServicePool.h"

using message::GetChatServerReq;
using message::GetChatServerRsp;
using message::StatusService;

// 压力测试用户注册 + 获取真实 token 的完整流程
// 返回有效的 token（从 StatusServer 获取），供 chatLogin 使用
inline TestAccount acquireTestAccount(const std::string& tag = "perf") {
    // 1. 通过 GateServer 注册内部测试用户（写验证码到 Redis + 落库 MySQL）
    AccountManager mgr;
    auto acct = mgr.acquire(tag);

    // 2. 登录 GateServer，获取真实 token
    mgr.login(acct);
    return acct;
}

// 带真实 token 的登录（注册 + 取 token + 登录一步完成）
inline bool loginWithRealToken(std::shared_ptr<ChatTestClient> client, const std::string& tag = "perf") {
    const auto acct = acquireTestAccount(tag);
    client->setAccount(acct);
    client->start();
    return true;
}

#endif
