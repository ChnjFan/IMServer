//
// Created by Fan on 2026/4/30.
//

#include <iostream>

#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>

#include "const.h"
#include "LogicSystem.h"
#include "VerifyGrpcClient.h"
#include "StatusGrpcClient.h"
#include "RedisMgr.h"
#include "MysqlMgr.h"

LogicSystem::~LogicSystem() {
}

bool LogicSystem::handleGet(const std::string& path, const std::shared_ptr<HttpConnection>& connection) {
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

bool LogicSystem::handlePost(const std::string& path, const std::shared_ptr<HttpConnection>& connection) {
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
        beast::ostream(connection->response_.body()) << "receive get_test request.\r\n";
        UrlParams urlParams = connection->urlParser_.getParams();
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
            root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
            const std::string jsonStr = root.toStyledString();
            boost::beast::ostream(connection->response_.body()) << jsonStr;
            return;
        }

        if (!srcRoot.isMember("email") || !srcRoot["email"].isString()) {
            std::cout << "Failed to parse JSON data" << std::endl;
            root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
            const std::string jsonStr = root.toStyledString();
            boost::beast::ostream(connection->response_.body()) << jsonStr;
            return;
        }

        auto email = srcRoot["email"].asString();
        std::cout << "email is: " << email << std::endl;

        // 给验证服务发送验证码请求
        GetVerifyRsp response = VerifyGrpcClient::getInstance()->GetVerifyCode(email);

        root["error"] = response.error();
        root["email"] = email;

        const std::string jsonStr = root.toStyledString();
        std::cout << "Response: " << jsonStr << std::endl;
        boost::beast::ostream(connection->response_.body()) << jsonStr;
    });

    // 注册请求
    registerPost("/user_register", [](std::shared_ptr<HttpConnection> connection) {
        auto bodyString = boost::beast::buffers_to_string(connection->request_.body().data());
        std::cout << "receive body: " << bodyString << std::endl;

        connection->response_.set(http::field::content_type, "application/json");
        Json::Value root;
        Json::Value srcRoot;
        if (Json::Reader reader; !reader.parse(bodyString, srcRoot)) {
            std::cout << "Failed to parse JSON data" << std::endl;
            root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
            const std::string jsonStr = root.toStyledString();
            boost::beast::ostream(connection->response_.body()) << jsonStr;
            return;
        }

        // 先校验验证码是否正确
        auto email = srcRoot["email"].asString();
        auto codeEmail = CODE_PREFIX + email;
        auto verifyCode = srcRoot["verify_code"].asString();
        std::string expectCode;
        if (auto res = RedisMgr::getInstance()->get(codeEmail, expectCode); !res) {
            std::cout << "Verify code expired" << std::endl;
            root["error"] = static_cast<int32_t>(ErrorCodes::VERIFY_CODE_EXPIRED);
            const std::string jsonStr = root.toStyledString();
            boost::beast::ostream(connection->response_.body()) << jsonStr;
            return;
        }

        if (verifyCode != expectCode) {
            std::cout << "Invalid verify code, expect: " << expectCode << std::endl;
            root["error"] = static_cast<int32_t>(ErrorCodes::VERIFY_CODE_NOT_REACHED);
            const std::string jsonStr = root.toStyledString();
            boost::beast::ostream(connection->response_.body()) << jsonStr;
            return;
        }

        // MySql 中查找并注册用户
        auto user = srcRoot["user"].asString();
        auto passwd = srcRoot["password"].asString();
        auto confirm = srcRoot["confirm"].asString();
        if (passwd != confirm) {
            std::cout << "passwd and confirm is not match" << std::endl;
            root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
            const std::string jsonStr = root.toStyledString();
            boost::beast::ostream(connection->response_.body()) << jsonStr;
            return;
        }
        int uid = MysqlMgr::getInstance()->registerUser(user, email, passwd);
        if (uid == 0 || uid == -1) {
            std::cout << "Register user email or name exist" << std::endl;
            root["error"] = static_cast<int32_t>(ErrorCodes::USER_EXISTS);
            const std::string jsonStr = root.toStyledString();
            boost::beast::ostream(connection->response_.body()) << jsonStr;
            return;
        }

        // 给验证服务发送验证码请求
        root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
        root["email"] = email;
        root["uid"] = uid;
        root["user"] = user;
        root["passwd"] = passwd;
        root["confirm"] = confirm;
        root["verify_code"] = verifyCode;

        const std::string jsonStr = root.toStyledString();
        std::cout << "Response: " << jsonStr << std::endl;
        boost::beast::ostream(connection->response_.body()) << jsonStr;
    });

    // 重置密码
    registerPost("/reset_passwd", [](std::shared_ptr<HttpConnection> connection) {
        auto bodyString = boost::beast::buffers_to_string(connection->request_.body().data());
        std::cout << "receive body: " << bodyString << std::endl;

        connection->response_.set(http::field::content_type, "application/json");
        Json::Value root;
        Json::Value srcRoot;
        if (Json::Reader reader; !reader.parse(bodyString, srcRoot)) {
            std::cout << "Failed to parse JSON data" << std::endl;
            root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
            const std::string jsonStr = root.toStyledString();
            boost::beast::ostream(connection->response_.body()) << jsonStr;
            return;
        }

        // 先校验验证码是否正确
        auto email = srcRoot["email"].asString();
        auto codeEmail = CODE_PREFIX + email;
        auto verifyCode = srcRoot["verify_code"].asString();
        std::string expectCode;
        if (auto res = RedisMgr::getInstance()->get(codeEmail, expectCode); !res) {
            std::cout << "Verify code expired" << std::endl;
            root["error"] = static_cast<int32_t>(ErrorCodes::VERIFY_CODE_EXPIRED);
            const std::string jsonStr = root.toStyledString();
            boost::beast::ostream(connection->response_.body()) << jsonStr;
            return;
        }

        if (verifyCode != expectCode) {
            std::cout << "Invalid verify code, expect: " << expectCode << std::endl;
            root["error"] = static_cast<int32_t>(ErrorCodes::VERIFY_CODE_NOT_REACHED);
            const std::string jsonStr = root.toStyledString();
            boost::beast::ostream(connection->response_.body()) << jsonStr;
            return;
        }

        // 数据库校验用户名和邮箱是否匹配
        if (bool result = MysqlMgr::getInstance()->checkEmail(email); !result) {
            std::cout << "User email not match" << std::endl;
            root["error"] = static_cast<int32_t>(ErrorCodes::USER_EMAIL_NOT_EXISTS);
            const std::string jsonStr = root.toStyledString();
            boost::beast::ostream(connection->response_.body()) << jsonStr;
            return;
        }

        auto passwd = srcRoot["password"].asString();
        if (bool result = MysqlMgr::getInstance()->updatePasswd(email, passwd); !result) {
            std::cout << "User email not match" << std::endl;
            root["error"] = static_cast<int32_t>(ErrorCodes::USER_EMAIL_NOT_EXISTS);
            const std::string jsonStr = root.toStyledString();
            boost::beast::ostream(connection->response_.body()) << jsonStr;
            return;
        }

        // 给验证服务发送验证码请求
        root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
        root["email"] = email;
        root["passwd"] = passwd;
        root["verify_code"] = verifyCode;

        const std::string jsonStr = root.toStyledString();
        std::cout << "Response: " << jsonStr << std::endl;
        boost::beast::ostream(connection->response_.body()) << jsonStr;
    });

    // 登录请求
    registerPost("/user_login", [](std::shared_ptr<HttpConnection> connection) {
        auto bodyString = boost::beast::buffers_to_string(connection->request_.body().data());
        std::cout << "receive body: " << bodyString << std::endl;

        connection->response_.set(http::field::content_type, "application/json");
        Json::Value root;
        Json::Value srcRoot;
        Defer defer([&root, &connection] {
            const std::string jsonStr = root.toStyledString();
            std::cout << "Response: " << jsonStr << std::endl;
            boost::beast::ostream(connection->response_.body()) << jsonStr;
        });
        if (Json::Reader reader; !reader.parse(bodyString, srcRoot)) {
            std::cout << "Failed to parse JSON data" << std::endl;
            root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
            return;
        }

        // 先校验验证码是否正确
        auto email = srcRoot["email"].asString();
        auto passwd = srcRoot["password"].asString();

        // 数据库校验邮箱和密码是否匹配，获取用户信息
        UserInfo userInfo;
        if (bool result = MysqlMgr::getInstance()->checkPasswd(email, passwd, userInfo); !result) {
            std::cout << "User passwd not match" << std::endl;
            root["error"] = static_cast<int32_t>(ErrorCodes::USER_EMAIL_NOT_EXISTS);
            return;
        }

        // 根据 uid 获取 ChatServer 连接
        auto reply = StatusGrpcClient::getInstance()->GetChatServer(userInfo.uid);
        if (reply.error()) {
            std::cout << "GRPC Status Client error: " << reply.error() << std::endl;
            root["error"] = static_cast<int32_t>(ErrorCodes::RPC_FAILED);
            return;
        }

        if (reply.host().empty() || reply.port().empty()) {
            std::cout << "GRPC Status Client not found chatserver" << std::endl;
            root["error"] = static_cast<int32_t>(ErrorCodes::RPC_FAILED);
            return;
        }

        auto resourceServer = StatusGrpcClient::getInstance()->GetResourceServer(userInfo.uid);
        if (resourceServer.error()) {
            std::cout << "GRPC Resource Server error: " << resourceServer.error() << std::endl;
            root["error"] = static_cast<int32_t>(ErrorCodes::RPC_FAILED);
            return;
        }
        if (resourceServer.host().empty() || resourceServer.port().empty()) {
            std::cout << "GRPC Resource Server not found" << std::endl;
            root["error"] = static_cast<int32_t>(ErrorCodes::RPC_FAILED);
            return;
        }

        // 给验证服务发送验证码请求
        root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
        root["uid"] = std::to_string(userInfo.uid);
        root["token"] = reply.token();
        root["host"] = reply.host();
        root["port"] = reply.port();
        root["resource_host"] = resourceServer.host();
        root["resource_port"] = resourceServer.port();
    });
}
