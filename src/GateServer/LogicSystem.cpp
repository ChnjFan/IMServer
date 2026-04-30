//
// Created by Fan on 2026/4/30.
//

#include "LogicSystem.h"

LogicSystem::~LogicSystem() {
}

bool LogicSystem::handleGet(std::string url, std::shared_ptr<HttpConnection> connection) {
    if (getHandlers_.find(url) == getHandlers_.end()) {
        return false;
    }

    getHandlers_[url](connection);
    return true;
}

void LogicSystem::registerGet(const std::string& url, const HttpRequestCallback& handler) {
    if (getHandlers_.count(url)) return;
    getHandlers_.insert(std::make_pair(url, handler));
}

LogicSystem::LogicSystem() {
    registerGet("/get_test", [](std::shared_ptr<HttpConnection> connection) {
        beast::ostream(connection->response_.body()) << "receive get_test request\r\n";
    });
}
