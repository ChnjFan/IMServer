//
// Created by Fan on 2026/5/11.
//

#include <iostream>

#include "ChatServer.h"

#include "AsioIOServicePool.h"
#include "UserMgr.h"
#include "ConfigMgr.h"
#include "DistLock.h"
#include "RedisMgr.h"

ChatServer::ChatServer(net::io_context &io_context, const unsigned short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    , ioContext_(io_context)
    , timer_(ioContext_) {
}

void ChatServer::start() {
    auto self = shared_from_this();

    // 服务器定时任务
    timerJob();

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

void ChatServer::timerJob() {
    auto self = shared_from_this();
    timer_.expires_after(std::chrono::seconds(CHAT_SERVER_TIMER_DEFAULT_EXPIRE));
    timer_.async_wait([self, this](const boost::system::error_code& error) {
        if (error) {
            std::cout << "[ChatServer] Server timer Error: " << error.message() << std::endl;
            return;
        }

        checkSessionHeartbeat();
        updateServerCount();

        timerJob();
    });
}

void ChatServer::checkSessionHeartbeat() {
    const auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto & [sessionId, session] : sessions_) {
            if (session->isSessionExpire(now)) {
                expireSessions_.push_back(session);
            }
        }
    }
    // 处理过期连接
    for (const auto& session : expireSessions_) {
        std::cout << "[ChatServer] Session ID: " << session->getSessionId()
                << " UID: " << session->getUserId() << " Expired" << std::endl;
        session->updateState(SessionState::OFFLINE);
        session->close();
        clearSession(session->getSessionId());
    }
    expireSessions_.clear();
}

void ChatServer::updateServerCount() const {
    const int count = static_cast<int>(sessions_.size());
    const auto serverName = ConfigMgr::getInstance().getValue("ChatServer", "Name");
    DistLockGuard lockServer(DIST_LOCK_PREFIX + serverName, DIST_LOCK_TIMEOUT, DIST_ACQUIRE_TIMEOUT);
    // 更新登录数量
    RedisMgr::getInstance()->hSet(LOGIN_COUNT, serverName, std::to_string(count));
}


