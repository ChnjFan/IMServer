//
// Created by Fan on 2026/5/10.
//

#include "StatusServiceImpl.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "const.h"
#include "ConfigMgr.h"
#include "RedisMgr.h"

using boost::uuids::uuid;
using boost::uuids::random_generator;

StatusServiceImpl::StatusServiceImpl() {
    auto& config = ConfigMgr::getInstance();
    if (config["ChatServers"]["Name"].empty()) {
        std::cout << "ChatServers not config" << std::endl;
        return;
    }

    std::stringstream ss(config["ChatServers"]["Name"]);
    std::string serverName;
    while (std::getline(ss, serverName, ',')) {
        if (serverName.empty()) {
            continue;
        }
        ChatServerInfo server;
        server.name = serverName;
        server.host = config[serverName]["Host"];
        server.port = config[serverName]["Port"];
        server.connCount = 0;
        chatServers_.insert({serverName, server});
    }
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
    const auto countStr = RedisMgr::getInstance()->hGet(LOGIN_COUNT, minServer.name);
    if (countStr.empty()) {// 没有找到服务器可能没有开
        minServer.connCount = INT_MAX;
    }
    else {
        minServer.connCount = std::stoi(countStr);
    }

    for (auto& [name, server] : chatServers_) {
        if (name == minServer.name) {
            continue;
        }

        const auto countStr = RedisMgr::getInstance()->hGet(LOGIN_COUNT, name);
        if (countStr.empty()) {// 没有找到服务器可能没有开
            server.connCount = INT_MAX;
        }
        else {
            server.connCount = std::stoi(countStr);
        }

        if (server.connCount < minServer.connCount) {
            minServer = server;
        }
    }
    return minServer;
}

void StatusServiceImpl::insertToken(const int uid, const std::string& token) {
    std::cout << "Insert [uid: " << uid << ", token: " << token << "]" << std::endl;
    RedisMgr::getInstance()->set(USER_TOKEN_PREFIX + std::to_string(uid), token);
}

bool StatusServiceImpl::checkToken(const int uid, const std::string &token) {
    if (token.empty()) {
        return false;
    }
    std::string expect;
    if (!RedisMgr::getInstance()->get(USER_TOKEN_PREFIX + std::to_string(uid), expect)) {
        std::cout << "Check [uid: " << uid << "not found" << std::endl;
        return false;
    }
    std::cout << "Check [uid: " << uid << ", token: " << token << "]" << std::endl;
    return expect == token;
}
