//
// Created by Fan on 2026/4/30.
//

#include <iostream>

#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>

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
    if (postHandlers_.find(path) == postHandlers_.end()) {
        return false;
    }

    postHandlers_[path](connection);
    return true;
}

void LogicSystem::registerPost(const std::string &path, const HttpRequestCallback &handler) {
    if (postHandlers_.count(path)) return;
    postHandlers_.insert(std::make_pair(path, handler));
}

LogicSystem::LogicSystem() {
    registerGet("/get_test", [](std::shared_ptr<HttpConnection> connection) {
        beast::ostream(connection->response_.body()) << "receive get_test request\r\n";
        HttpConnection::UrlParams urlParams = connection->urlParser_.getParams();
        for (auto&[param, value] : urlParams) {
            beast::ostream(connection->response_.body()) << "Param " << param << "=" << value << "\r\n";
        }
    });

    // 获取验证码
    registerPost("/get_verify_code", [](std::shared_ptr<HttpConnection> connection) {
        auto bodyString = boost::beast::buffers_to_string(connection->request_.body().data());
        std::cout << "receive body: " << bodyString << std::endl;

        connection->response_.set(http::field::content_type, "application/json");
        Json::Value root;
        Json::Value srcRoot;
        if (Json::Reader reader; !reader.parse(bodyString, srcRoot)) {
            std::cout << "Failed to parse JSON data" << std::endl;
            root["error"] = static_cast<int16_t>(ErrorCodes::ERROR_JSON);
            const std::string jsonStr = root.toStyledString();
            boost::beast::ostream(connection->response_.body()) << jsonStr;
            return;
        }

        if (!srcRoot.isMember("email") || !srcRoot["email"].isString()) {
            std::cout << "Failed to parse JSON data" << std::endl;
            root["error"] = static_cast<int16_t>(ErrorCodes::ERROR_JSON);
            const std::string jsonStr = root.toStyledString();
            boost::beast::ostream(connection->response_.body()) << jsonStr;
            return;
        }

        auto email = srcRoot["email"].asString();
        std::cout << "email is: " << email << std::endl;
        root["error"] = static_cast<int16_t>(ErrorCodes::SUCCESS);
        root["email"] = email;

        const std::string jsonStr = root.toStyledString();
        boost::beast::ostream(connection->response_.body()) << jsonStr;
    });
}
