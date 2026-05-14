//
// Created by Fan on 2026/5/10.
//

#include "StatusServiceImpl.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "const.h"
#include "ConfigMgr.h"

using boost::uuids::uuid;
using boost::uuids::random_generator;

StatusServiceImpl::StatusServiceImpl() {
    auto& config = ConfigMgr::getInstance();
    ChatServerInfo server;
    server.name = "ChatServer";
    server.host = config["ChatServer"]["Host"];
    server.port = config["ChatServer"]["Port"];
    server.connCount = 0;
    chatServers_.insert({"ChatServer", server});
}

Status StatusServiceImpl::GetChatServer(ServerContext *context, const GetChatServerReq *request,
                                        GetChatServerRsp *response) {
    const auto& server = getChatServerInfo();
    response->set_error(static_cast<int32_t>(ErrorCodes::SUCCESS));
    response->set_host(server.host);
    response->set_port(server.port);

    random_generator generator;
    std::string token = boost::uuids::to_string(generator());
    response->set_token(token);
    // 后续登录校验
    insertToken(request->uid(), response->token());
    return Status::OK;
}

Status StatusServiceImpl::Login(ServerContext *context, const message::LoginReq *request, message::LoginRsp *response) {
    const auto uid = request->uid();
    const auto& token = request->token();

    response->set_error(static_cast<int32_t>(ErrorCodes::SUCCESS));
    if (!checkToken(uid, token)) {
        response->set_error(static_cast<int32_t>(ErrorCodes::CHAT_LOGIN_TOKEN_ERROR));
        return Status::OK;
    }
    response->set_uid(uid);
    response->set_token(token);
    return Status::OK;
}

ChatServerInfo StatusServiceImpl::getChatServerInfo() {
    std::lock_guard<std::mutex> lock(serverMutex_);
    if (chatServers_.empty()) {
        std::cout << "Not found ChatServer" << std::endl;
        return {};
    }
    auto minServer = chatServers_.begin()->second;
    for (auto& [name, server] : chatServers_) {
        if (server.connCount < minServer.connCount) {
            minServer = server; // 找到最小负载的服务
        }
    }
    return minServer;
}

void StatusServiceImpl::insertToken(int uid, const std::string& token) {
    std::lock_guard<std::mutex> lock(tokenMutex_);
    std::cout << "Insert [uid: " << uid << ", token: " << token << "]" << std::endl;
    if (tokens_.find(uid) != tokens_.end()) {
        std::cout << "erase old token" << std::endl;
        tokens_.erase(uid);
    }
    tokens_.insert({uid, token});
}

bool StatusServiceImpl::checkToken(const int uid, const std::string &token) {
    std::lock_guard<std::mutex> lock(tokenMutex_);
    if (tokens_.find(uid) == tokens_.end()) {
        std::cout << "Check [uid: " << uid << "not found" << std::endl;
        return false;
    }
    std::cout << "Check [uid: " << uid << ", token: " << token << "]" << std::endl;
    return tokens_[uid] == token;
}
