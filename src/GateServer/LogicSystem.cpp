//
// Created by Fan on 2026/4/30.
//

#include "LogicSystem.h"

LogicSystem::~LogicSystem() {
}

bool LogicSystem::handleGet(const std::string& path, std::shared_ptr<HttpConnection> connection) {
    if (getHandlers_.find(path) == getHandlers_.end()) {
        return false;
    }

    getHandlers_[path](connection);
    return true;
}

void LogicSystem::registerGet(const std::string& path, const HttpRequestCallback& handler) {
    if (getHandlers_.count(path)) return;
    getHandlers_.insert(std::make_pair(path, handler));
}

bool LogicSystem::handlePost(const std::string& path, std::shared_ptr<HttpConnection> connection) {
    return true;
}

void LogicSystem::registerPost(const std::string &path, const HttpRequestCallback &handler) {
}

LogicSystem::LogicSystem() {
    registerGet("/get_test", [](std::shared_ptr<HttpConnection> connection) {
        beast::ostream(connection->response_.body()) << "receive get_test request\r\n";
        HttpConnection::UrlParams urlParams = connection->urlParser_.getParams();
        for (auto&[param, value] : urlParams) {
            beast::ostream(connection->response_.body()) << "Param " << param << "=" << value << "\r\n";
        }
    });
}
