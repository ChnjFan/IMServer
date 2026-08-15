//
// Created by Fan on 2026/7/2.
//

#include "ResourceLogicSystem.h"

#include <utility>

#include "service/ChunkUploadHandler.h"
#include "service/PreCheckHandler.h"
#include "service/DownloadHandler.h"

bool ResourceLogicSystem::handleGet(const std::string& path, const std::shared_ptr<HttpConnection>& connection) {
    if (getHandlers_.find(path) == getHandlers_.end()) {
        return false;
    }

    getHandlers_[path](connection);
    return true;
}

void ResourceLogicSystem::registerGet(const std::string& path, const ResourceRequestCallback& handler) {
    if (getHandlers_.count(path)) return;
    getHandlers_.insert(std::make_pair(path, handler));
}

bool ResourceLogicSystem::handlePost(const std::string& path, const std::shared_ptr<HttpConnection>& connection) {
    if (postHandlers_.find(path) == postHandlers_.end()) {
        return false;
    }

    postHandlers_[path](connection);
    return true;
}

void ResourceLogicSystem::registerPost(const std::string &path, const ResourceRequestCallback &handler) {
    if (postHandlers_.count(path)) return;
    postHandlers_.insert(std::make_pair(path, handler));
}

ResourceLogicSystem::ResourceLogicSystem() {
    // 分片上传入口 (init)
    registerPost("/r/upload", [](std::shared_ptr<HttpConnection> connection) {
        ChunkUploadHandler::handleInit(connection);
    });
    registerPost("/r/upload/init", [](std::shared_ptr<HttpConnection> connection) {
        ChunkUploadHandler::handleInit(connection);
    });

    // 分片上传 (chunk)
    registerPost("/r/upload/chunk", [](std::shared_ptr<HttpConnection> connection) {
        ChunkUploadHandler::handleChunk(connection);
    });

    // 断点续传状态查询
    registerPost("/r/upload/status", [](std::shared_ptr<HttpConnection> connection) {
        ChunkUploadHandler::handleStatus(connection);
    });

    // 分片上传完成 (finalize)
    registerPost("/r/upload/finalize", [](std::shared_ptr<HttpConnection> connection) {
        ChunkUploadHandler::handleFinalize(connection);
    });

    // Pre-check (秒传) endpoint
    registerPost("/r/check", [](std::shared_ptr<HttpConnection> connection) {
        PreCheckHandler handler;
        handler.handle(connection);
    });

    // Download endpoint (matches /r?resource_id={resource_id} and /r?resource_id={resource_id},thumb)
    registerGet("/r", [](std::shared_ptr<HttpConnection> connection) {
        DownloadHandler handler;
        handler.handle(connection);
    });
}
