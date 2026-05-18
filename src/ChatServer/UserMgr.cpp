//
// Created by Fan on 2026/5/18.
//

#include "UserMgr.h"

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

void UserMgr::removeUserSession(const int uid) {
    std::lock_guard<std::mutex> lock(mutex_session_);
    session_map_.erase(uid);
}

UserMgr::UserMgr() = default;
