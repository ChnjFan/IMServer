//
// Created by Fan on 2026/5/18.
//

#ifndef IMSERVER_USERMGR_H
#define IMSERVER_USERMGR_H

#include <mutex>
#include <unordered_map>

#include "Singleton.h"

class Session;
class UserMgr : public Singleton<UserMgr> {
public:
    ~UserMgr();
    std::shared_ptr<Session> getSession(int uid);
    void setUserSession(int uid, std::shared_ptr<Session> session);
    void removeUserSession(int uid);

private:
    friend class Singleton<UserMgr>;

    UserMgr();

    std::mutex mutex_session_;
    std::unordered_map<int, std::shared_ptr<Session>> session_map_;
};


#endif //IMSERVER_USERMGR_H