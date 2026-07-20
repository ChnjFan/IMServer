//
// Created by Fan on 2026/5/10.
//

#include "StatusServiceImpl.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "const.h"
#include "RedisMgr.h"
#include "DistLock.h"

using boost::uuids::uuid;
using boost::uuids::random_generator;

StatusServiceImpl::StatusServiceImpl() {
    auto& config = ConfigMgr::getInstance();
    if (config["ChatServers"]["Name"].empty()) {
        std::cout << "ChatServers not config" << std::endl;
        return;
    }

    initChatServer(config);
    initResourceServer(config);
}

Status StatusServiceImpl::GetResourceServer(ServerContext *context, const GetResourceServerReq *request,
                                            GetResourceServerRsp *response) {
    auto& config = ConfigMgr::getInstance();
    response->set_error(static_cast<int32_t>(ErrorCodes::SUCCESS));
    response->set_host(config["ResourceServer"]["Host"]);
    response->set_port(config["ResourceServer"]["Port"]);
    return Status::OK;
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

Status StatusServiceImpl::VerifyToken(ServerContext *context, const VerifyTokenReq *request, VerifyTokenRsp *response) {
    const auto uid = request->uid();
    const auto& token = request->token();

    response->set_error(static_cast<int32_t>(ErrorCodes::SUCCESS));
    response->set_uid(uid);

    if (checkToken(uid, token)) {
        response->set_error(static_cast<int32_t>(ErrorCodes::SUCCESS));
    } else {
        response->set_error(static_cast<int32_t>(ErrorCodes::RESOURCE_AUTH_FAILED));
    }
    return Status::OK;
}

void StatusServiceImpl::initChatServer(ConfigMgr &config) {
    std::stringstream ss(config["ChatServers"]["Name"]);
    std::string serverName;
    while (std::getline(ss, serverName, ',')) {
        if (serverName.empty()) {
            continue;
        }
        ServerInfo server;
        server.name = serverName;
        server.host = config[serverName]["Host"];
        if (config["Nginx"][serverName].empty()) {
            server.port = config[serverName]["Port"];
        }
        else {
            server.port = config["Nginx"][serverName];
        }
        server.connCount = 0;
        chatServers_.insert({serverName, server});
    }
}

void StatusServiceImpl::initResourceServer(ConfigMgr &config) {
    std::stringstream ss(config["ChatServers"]["Name"]);
    std::string serverName;
    while (std::getline(ss, serverName, ',')) {
        if (serverName.empty()) {
            continue;
        }
        ServerInfo server;
        server.name = serverName;
        server.host = config[serverName]["Host"];
        server.port = config[serverName]["Port"];
        server.httpPort = config[serverName]["HttpPort"];
        server.connCount = 0;
        resourceServers_.insert({serverName, server});
    }
}

ServerInfo StatusServiceImpl::getChatServerInfo() {
    std::lock_guard<std::mutex> lock(serverMutex_);
    if (chatServers_.empty()) {
        std::cout << "Not found ChatServer" << std::endl;
        return {};
    }
    // 同时对在线服务计数读写需要加分布式锁
    auto minServer = chatServers_.begin()->second;
    {
        DistLockGuard lockServer(DIST_LOCK_PREFIX + minServer.name, DIST_LOCK_TIMEOUT, DIST_ACQUIRE_TIMEOUT);
        if (const auto countStr = RedisMgr::getInstance()->hGet(LOGIN_COUNT, minServer.name); countStr.empty()) {
            // 没有找到服务器可能没有开
            minServer.connCount = INT_MAX;
        }
        else {
            minServer.connCount = std::stoi(countStr);
        }
    }

    for (auto& [name, server] : chatServers_) {
        if (name == minServer.name) {
            continue;
        }

        DistLockGuard lockServer(DIST_LOCK_PREFIX + name, DIST_LOCK_TIMEOUT, DIST_ACQUIRE_TIMEOUT);
        if (const auto count = RedisMgr::getInstance()->hGet(LOGIN_COUNT, name); count.empty()) {
            server.connCount = INT_MAX;
        }
        else {
            server.connCount = std::stoi(count);
        }

        if (server.connCount < minServer.connCount) {
            minServer = server;
        }
    }
    return minServer;
}

void StatusServiceImpl::insertToken(const int uid, const std::string& token) {
    std::cout << "Insert [uid: " << uid << ", token: " << token << "]" << std::endl;
    RedisMgr::getInstance()->hSet(USER_ONLINE_INFO_PREFIX+ std::to_string(uid),USER_ONLINE_TOKEN, token);
}

bool StatusServiceImpl::checkToken(const int uid, const std::string &token) {
    if (token.empty()) {
        return false;
    }
    const auto expect = RedisMgr::getInstance()->hGet(
        USER_ONLINE_INFO_PREFIX + std::to_string(uid), USER_ONLINE_TOKEN);
    if (expect.empty()) {
        std::cout << "Check [uid: " << uid << "] not found" << std::endl;
        return false;
    }
    return expect == token;
}
