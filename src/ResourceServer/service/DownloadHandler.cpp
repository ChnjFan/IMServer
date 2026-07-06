//
// Created by Fan on 2026/7/2.
//

#include "DownloadHandler.h"

#include <filesystem>
#include <fstream>
#include <iostream>

#include <fstream>

#include <json/json.h>

#include "ConfigMgr.h"
#include "const.h"
#include "common/model/ResourceMeta.h"
#include "core/ResourceMetaMgr.h"
#include "service/AuthMiddleware.h"

void DownloadHandler::handle(std::shared_ptr<HttpConnection> conn) {
    const auto& req = conn->getRequest();
    auto& resp = conn->getResponse();

    bool wantThumb = false;
    const std::string resourceId = extractResourceId(conn->getUrlParams(), wantThumb);

    if (resourceId.empty()) {
        sendError(conn, static_cast<int>(ErrorCodes::RESOURCE_NOT_FOUND),
                  "Invalid resource path", http::status::bad_request);
        return;
    }

    const auto auth = AuthMiddleware::authenticate(req);
    if (auth.error != 0) {
        sendError(conn, auth.error, "Unauthorized", http::status::unauthorized);
        return;
    }

    ResourceMeta meta;
    if (!ResourceMetaMgr::getInstance()->getResource(resourceId, meta)) {
        sendError(conn, static_cast<int>(ErrorCodes::RESOURCE_NOT_FOUND),
                  "Resource not found", http::status::not_found);
        return;
    }

    if (!auth.convId.empty() && meta.convId != auth.convId) {
        sendError(conn, static_cast<int>(ErrorCodes::RESOURCE_ACCESS_DENIED),
                  "Not a conversation member", http::status::forbidden);
        return;
    }

    const std::string rootPath = ConfigMgr::getInstance()["ResourceServer"]["Path"];
    std::string filePath;

    if (wantThumb) {
        if (meta.thumbPath.empty()) {
            sendError(conn, static_cast<int>(ErrorCodes::RESOURCE_NOT_FOUND),
                      "Thumbnail not available", http::status::not_found);
            return;
        }
        filePath = rootPath + "/" + meta.thumbPath;
    } else {
        filePath = rootPath + "/" + meta.filePath;
    }

    if (!std::filesystem::exists(filePath)) {
        sendError(conn, static_cast<int>(ErrorCodes::RESOURCE_NOT_FOUND),
                  "File not found on disk", http::status::not_found);
        return;
    }

    // 7. Serve file
    serveFile(conn, filePath, meta.fileName, meta.md5);
}

std::string DownloadHandler::extractResourceId(const UrlParams& params, bool& wantThumb) {
    // Path format: /r?resource_id={resource_id} or /r?resource_id={resource_id}&thumb=true
    // Check for /thumb suffix
    if (params.find("thumb") != params.end() && params.at("thumb") == "true") {
        wantThumb = true;
    }

    if (params.find("resource_id") != params.end()) {
        return params.at("resource_id");
    }

    return "";
}

std::string DownloadHandler::getMimeType(const std::string& filename) {
    size_t dotPos = filename.rfind('.');
    if (dotPos == std::string::npos) return "application/octet-stream";

    std::string ext = filename.substr(dotPos + 1);
    for (auto& c : ext) c = static_cast<char>(std::tolower(c));

    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "png") return "image/png";
    if (ext == "gif") return "image/gif";
    if (ext == "webp") return "image/webp";
    if (ext == "bmp") return "image/bmp";
    if (ext == "mp4") return "video/mp4";
    if (ext == "mp3") return "audio/mpeg";
    if (ext == "pdf") return "application/pdf";
    if (ext == "txt") return "text/plain";
    if (ext == "json") return "application/json";

    return "application/octet-stream";
}

void DownloadHandler::sendError(std::shared_ptr<HttpConnection> conn, int error,
                                 const std::string& message, http::status status) {
    auto& resp = conn->getResponse();
    resp.set(http::field::content_type, "application/json");
    resp.result(status);

    Json::Value root;
    root["error"] = error;
    root["message"] = message;
    beast::ostream(resp.body()) << root.toStyledString();
}

void DownloadHandler::serveFile(std::shared_ptr<HttpConnection> conn,
                                 const std::string& filePath,
                                 const std::string& fileName,
                                 const std::string& md5) {
    auto& resp = conn->getResponse();

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        sendError(conn, static_cast<int>(ErrorCodes::FILE_ERROR),
                  "Cannot open file", http::status::internal_server_error);
        return;
    }

    auto fileSize = std::filesystem::file_size(filePath);

    resp.set(http::field::content_type, getMimeType(fileName));
    resp.set(http::field::accept_ranges, "bytes");
    resp.set("Cache-Control", "private, max-age=86400");
    resp.set("ETag", "\"" + md5 + "\"");
    resp.content_length(fileSize);
    resp.result(http::status::ok);

    constexpr size_t BUFFER_SIZE = 65536;
    std::vector<char> buffer(BUFFER_SIZE);

    // 使用 net::buffer_copy 将文件数据提交到 dynamic_body 的 multi_buffer
    // 每次读取一块写入一块，大文件不会一次性分配 fileSize 内存
    while (file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
        net::buffer_copy(resp.body().prepare(static_cast<size_t>(file.gcount())),
                         net::buffer(buffer.data(), static_cast<size_t>(file.gcount())));
        resp.body().commit(static_cast<size_t>(file.gcount()));
    }
}
