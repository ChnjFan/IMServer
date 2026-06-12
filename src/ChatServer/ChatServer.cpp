//
// Created by Fan on 2026/5/11.
//

#include <iostream>

#include "ChatServer.h"

#include "AsioIOServicePool.h"
#include "UserMgr.h"

ChatServer::ChatServer(net::io_context &io_context, const unsigned short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    , ioContext_(io_context) {

}

void ChatServer::start() {
    auto self = shared_from_this();
    auto& io_context = AsioIOServicePool::getInstance()->getIOService();
    // 创建一个 Session 会话等待 TCP 连接
    auto session = std::make_shared<Session>(io_context, self);
    acceptor_.async_accept(session->getSocket(), [self, session](const boost::system::error_code &ec) {
        try {
            if (ec) {
                self->start();
                return;
            }
            self->insertSession(session);
            session->start();
            self->start();
        } catch (const std::exception& e) {
            std::cout << e.what() << std::endl;
        }
    });
}

void ChatServer::insertSession(const std::shared_ptr<Session>& session) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string id = session->getSessionId();
    if (sessions_.find(id) != sessions_.end()) {
        // 关闭旧会话后重新插入会话
        sessions_.erase(id);
    }
    sessions_.insert({id, session});
}

void ChatServer::clearSession(const std::string &sessionId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sessions_.find(sessionId) != sessions_.end()) {
        UserMgr::getInstance()->removeUserSession(sessions_[sessionId]->getUserId(),
                                                  sessions_[sessionId]->getSessionId());
    }
    sessions_.erase(sessionId);
}


