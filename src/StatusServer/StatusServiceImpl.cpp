//
// Created by Fan on 2026/5/10.
//

#include "StatusServiceImpl.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "const.h"

using boost::uuids::uuid;
using boost::uuids::random_generator;

Status StatusServiceImpl::GetChatServer(ServerContext *context, const GetChatServerReq *request,
                                        GetChatServerRsp *response) {
    const auto& server = getChatServerInfo();
    response->set_error(static_cast<int32_t>(ErrorCodes::SUCCESS));
    response->set_host(server.host);
    response->set_port(server.port);

    random_generator generator;
    std::string token = boost::uuids::to_string(generator());
    response->set_token(token);
    insertToken(request->uid(), response->token());
    return Status::OK;
}

ChatServerInfo StatusServiceImpl::getChatServerInfo() {
    std::lock_guard<std::mutex> lock(serverMutex_);
    if (chatServers_.empty()) {
        return ChatServerInfo();
    }
    auto minServer = chatServers_.begin()->second;
    for (auto& [name, server] : chatServers_) {
        if (server.connCount < minServer.connCount) {
            minServer = server; // 找到最小负载的服务
        }
    }
    return minServer;
}

void StatusServiceImpl::insertToken(int uid, std::string token) {
    std::lock_guard<std::mutex> lock(tokenMutex_);
    tokens_.insert({uid, token});
}
