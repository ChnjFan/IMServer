//
// Created by Fan on 2026/5/11.
//

#ifndef IMSERVER_CHATSERVER_H
#define IMSERVER_CHATSERVER_H

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "const.h"
#include "Session.h"

class ChatServer : std::enable_shared_from_this<ChatServer> {
public:
    ChatServer(net::io_context &io_context, unsigned short port);
    void start();
    void insertSession(const std::shared_ptr<Session>& session);
    void clearSession(const std::string &sessionId);
private:
    tcp::acceptor acceptor_;
    net::io_context& ioContext_;

    std::unordered_map<std::string, std::shared_ptr<Session>> sessions_;

    std::mutex mutex_;
};


#endif //IMSERVER_CHATSERVER_H