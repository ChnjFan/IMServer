//
// Created by Fan on 2026/7/2.
//

#include "PreCheckHandler.h"

#include <iostream>

#include <json/json.h>
#include <json/reader.h>

#include "const.h"
#include "core/ResourceMetaMgr.h"
#include "service/AuthMiddleware.h"

void PreCheckHandler::handle(std::shared_ptr<HttpConnection> conn) {
    auto& req = conn->getRequest();
    auto& resp = conn->getResponse();
    resp.set(http::field::content_type, "application/json");

    Json::Value root;

    std::cout << "Received precheck request: " << conn->getRequest() << std::endl;

    // 1. Auth check
    auto auth = AuthMiddleware::authenticate(req);
    if (auth.error != 0) {
        root["error"] = auth.error;
        root["message"] = "Token expired or uid mismatch";
        beast::ostream(resp.body()) << root.toStyledString();
        resp.result(http::status::unauthorized);
        return;
    }

    // 2. Parse JSON body
    auto bodyStr = boost::beast::buffers_to_string(req.body().data());
    Json::Value srcRoot;
    if (Json::Reader reader; !reader.parse(bodyStr, srcRoot)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        root["message"] = "Failed to parse JSON";
        beast::ostream(resp.body()) << root.toStyledString();
        resp.result(http::status::bad_request);
        return;
    }

    if (!srcRoot.isMember("file_md5") || !srcRoot["file_md5"].isString()) {
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        root["message"] = "Missing file_md5 field";
        beast::ostream(resp.body()) << root.toStyledString();
        resp.result(http::status::bad_request);
        return;
    }

    std::string md5 = srcRoot["file_md5"].asString();

    // 3. Check if resource exists
    ResourceMeta meta;
    if (ResourceMetaMgr::getInstance()->preCheck(md5, meta)) {
        root["found"] = true;
        root["resource_id"] = meta.resourceId;
        root["url"] = "/r/" + meta.resourceId;
        if (!meta.thumbPath.empty()) {
            root["thumb_url"] = "/r/" + meta.resourceId + "/thumb";
        }
    } else {
        root["found"] = false;
    }

    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
    beast::ostream(resp.body()) << root.toStyledString();
    resp.result(http::status::ok);
}
