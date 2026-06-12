//
// Created by Fan on 2026/5/18.
//

#include "UserMgr.h"

#include "Session.h"

UserMgr::~UserMgr() = default;

std::shared_ptr<Session> UserMgr::getSession(const int uid) {
    std::lock_guard<std::mutex> lock(mutex_session_);
    const auto iter = session_map_.find(uid);
    if (iter == session_map_.end()) {
        return nullptr;
    }
    return iter->second;
}

void UserMgr::setUserSession(const int uid, std::shared_ptr<Session> session) {
    std::lock_guard<std::mutex> lock(mutex_session_);
    session_map_[uid] = session;
}

void UserMgr::removeUserSession(const int uid, std::string& sessionId) {
    std::lock_guard<std::mutex> lock(mutex_session_);
    auto iter = session_map_.find(uid);
    if (iter == session_map_.end()) {
        return;
    }
    if (sessionId != iter->second->getSessionId()) {
        // 其他终端已经登录
        return;
    }
    session_map_.erase(iter);
}

UserMgr::UserMgr() = default;
