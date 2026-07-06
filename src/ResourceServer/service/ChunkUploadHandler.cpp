//
// Created by Fan on 2026/7/3.
//

#include "ChunkUploadHandler.h"

#include <filesystem>
#include <fstream>
#include <iostream>

#include <json/json.h>
#include <json/reader.h>

#include "ConfigMgr.h"
#include "const.h"
#include "Md5.h"
#include "DistLock.h"
#include "common/ResourceConfig.h"
#include "common/model/ResourceMeta.h"
#include "core/ResourceMetaMgr.h"
#include "service/AuthMiddleware.h"
#include "service/UploadSessionMgr.h"

// 默认分片大小 2MB
static constexpr int DEFAULT_CHUNK_SIZE = 2 * 1024 * 1024;
// 合并文件时读取缓冲区 64KB
static constexpr size_t MERGE_READ_BUFFER = 64 * 1024;

void ChunkUploadHandler::sendJsonResponse(std::shared_ptr<HttpConnection> conn,
                                           http::status status, const Json::Value& body) {
    auto& resp = conn->getResponse();
    resp.set(http::field::content_type, "application/json");
    resp.result(status);
    beast::ostream(resp.body()) << body.toStyledString();
}

std::string ChunkUploadHandler::getUploadRootPath() {
    return ConfigMgr::getInstance()["ResourceServer"]["Path"];
}

std::string ChunkUploadHandler::getTmpRootPath() {
    return getUploadRootPath() + "/.upload_tmp";
}

// ============================================================
// POST /r/upload  (init)
// ============================================================
void ChunkUploadHandler::handleInit(std::shared_ptr<HttpConnection> conn) {
    auto& req = conn->getRequest();
    auto& resp = conn->getResponse();
    resp.set(http::field::content_type, "application/json");

    Json::Value root;
    Defer defer([&resp, &root]() {
        beast::ostream(resp.body()) << root.toStyledString();
    });

    std::cout << "Received init request: " << req << std::endl;

    // 权限校验 token 是否与 uid 一致
    if (auto auth = AuthMiddleware::authenticate(req); auth.error != 0) {
        root["error"] = auth.error;
        root["message"] = "Token expired or uid mismatch";
        resp.result(http::status::unauthorized);
        return;
    }

    auto bodyStr = boost::beast::buffers_to_string(req.body().data());
    Json::Value srcRoot;
    if (Json::Reader reader; !reader.parse(bodyStr, srcRoot)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        root["message"] = "Failed to parse JSON";
        resp.result(http::status::bad_request);
        return;
    }

    if (!srcRoot.isMember("conv_id")
        || !srcRoot.isMember("file_name")
        || !srcRoot.isMember("file_size")) {
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        root["message"] = "Missing required fields: conv_id, file_name, file_size";
        resp.result(http::status::bad_request);
        return;
    }

    std::string convId = srcRoot["conv_id"].asString();
    std::string fileName = srcRoot["file_name"].asString();
    std::string expectedMd5 = srcRoot.isMember("file_md5") ? srcRoot["file_md5"].asString() : "";
    int64_t fileSize = srcRoot["file_size"].asInt64();

    int chunkSize = srcRoot.isMember("chunk_size") ? srcRoot["chunk_size"].asInt() : DEFAULT_CHUNK_SIZE;
    if (chunkSize <= 0 || chunkSize > DEFAULT_CHUNK_SIZE * 4) {
        chunkSize = DEFAULT_CHUNK_SIZE; // 限制最大分片 8MB
    }

    // todo 这里先用了图片大小限制，后续要精细控制不同类型文件大小
    auto maxFileSize = ImageProcessConfig::fromConfigMgr().maxFileSize;
    if (fileSize <= 0 || fileSize > maxFileSize) {
        root["error"] = static_cast<int32_t>(ErrorCodes::RESOURCE_SIZE_EXCEEDED);
        root["message"] = "Invalid file size";
        resp.result(http::status::bad_request);
        return;
    }

    int totalChunks = static_cast<int>((fileSize + chunkSize - 1) / chunkSize);

    // 创建文件上传会话，状态保存在 Redis 中
    auto uploadId = UploadSessionMgr::getInstance()->create(
        convId, fileName, expectedMd5, fileSize, chunkSize, totalChunks);

    if (!uploadId.has_value()) {
        root["error"] = static_cast<int32_t>(ErrorCodes::REDIS_ERROR);
        root["message"] = "Failed to create upload session";
        resp.result(http::status::internal_server_error);
        return;
    }

    std::error_code ec;
    std::filesystem::create_directories(getTmpRootPath(), ec);

    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
    root["upload_id"] = uploadId.value();
    root["chunk_size"] = chunkSize;
    root["total_chunks"] = totalChunks;
    resp.result(http::status::ok);
}

// ============================================================
// POST /r/upload/chunk
// ============================================================
void ChunkUploadHandler::handleChunk(std::shared_ptr<HttpConnection> conn) {
    auto& req = conn->getRequest();
    auto& resp = conn->getResponse();
    resp.set(http::field::content_type, "application/json");

    Json::Value root;
    Defer defer([&resp, &root]() {
        beast::ostream(resp.body()) << root.toStyledString();
    });

    if (auto auth = AuthMiddleware::authenticate(req); auth.error != 0) {
        root["error"] = auth.error;
        root["message"] = "Token expired or uid mismatch";
        resp.result(http::status::unauthorized);
        return;
    }

    auto uploadId = AuthMiddleware::extractHeaderString(req, "X-Upload-Id");
    auto chunkIdxStr = AuthMiddleware::extractHeaderString(req, "X-Chunk-Index");
    auto chunkMd5 = AuthMiddleware::extractHeaderString(req, "X-Chunk-Md5");

    if (uploadId.empty() || chunkIdxStr.empty()) {
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        root["message"] = "Missing headers: X-Upload-Id, X-Chunk-Index";
        resp.result(http::status::bad_request);
        return;
    }

    int chunkIndex = std::stoi(chunkIdxStr);

    auto session = UploadSessionMgr::getInstance()->get(uploadId);
    if (!session.has_value()) {
        root["error"] = static_cast<int32_t>(ErrorCodes::RESOURCE_STALE_UPLOAD);
        root["message"] = "Upload session not found or expired";
        resp.result(http::status::gone); // 410
        return;
    }

    if (chunkIndex < 0 || chunkIndex >= session->totalChunks) {
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        root["message"] = "Invalid chunk index";
        resp.result(http::status::bad_request);
        return;
    }

    // 检查当前发送的 chunk 是否已经收到过
    if (UploadSessionMgr::getInstance()->isChunkReceived(uploadId, chunkIndex)) {
        auto status = UploadSessionMgr::getInstance()->queryStatus(uploadId);
        root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
        root["status"] = "already_exists";
        root["received"] = status->receivedCount;
        root["total"] = session->totalChunks;
        resp.result(http::status::ok);
        return;
    }

    // 5. Read body (bounded by chunk_size)
    auto bodyData = req.body().data();
    std::string bodyStr = boost::beast::buffers_to_string(bodyData);
    int64_t bodySize = static_cast<int64_t>(bodyStr.size());

    if (bodySize > session->chunkSize + 1024) { // 允许少量余量
        root["error"] = static_cast<int32_t>(ErrorCodes::RESOURCE_SIZE_EXCEEDED);
        root["message"] = "Chunk size exceeds limit";
        resp.result(http::status::bad_request);
        return;
    }

    // 6. Verify chunk MD5 (if provided)
    if (!chunkMd5.empty()) {
        std::string actualMd5 = Md5::compute(bodyStr);
        if (actualMd5 != chunkMd5) {
            root["error"] = static_cast<int32_t>(ErrorCodes::RESOURCE_MD5_MISMATCH);
            root["message"] = "Chunk md5 mismatch";
            resp.result(http::status::bad_request);
            return;
        }
    }

    // 7. Write to .part file
    std::string partPath = getTmpRootPath() + "/" + uploadId + "_" + std::to_string(chunkIndex) + ".part";
    {
        std::ofstream file(partPath, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            root["error"] = static_cast<int32_t>(ErrorCodes::FILE_ERROR);
            root["message"] = "Failed to write chunk file";
            resp.result(http::status::internal_server_error);
            return;
        }
        file.write(bodyStr.data(), bodyStr.size());
        if (file.fail()) {
            root["error"] = static_cast<int32_t>(ErrorCodes::FILE_ERROR);
            root["message"] = "Failed to write chunk data";
            resp.result(http::status::internal_server_error);
            return;
        }
    }

    // 8. Mark received
    UploadSessionMgr::getInstance()->markChunkReceived(uploadId, chunkIndex, chunkMd5);

    auto status = UploadSessionMgr::getInstance()->queryStatus(uploadId);
    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
    root["status"] = "ok";
    root["received"] = status->receivedCount;
    root["total"] = session->totalChunks;
    resp.result(http::status::ok);
}

// ============================================================
// POST /r/upload/status
// ============================================================
void ChunkUploadHandler::handleStatus(std::shared_ptr<HttpConnection> conn) {
    auto& req = conn->getRequest();
    auto& resp = conn->getResponse();
    resp.set(http::field::content_type, "application/json");

    Json::Value root;
    Defer defer([&resp, &root]() {
        beast::ostream(resp.body()) << root.toStyledString();
    });

    // 1. Auth check
    auto auth = AuthMiddleware::authenticate(req);
    if (auth.error != 0) {
        root["error"] = auth.error;
        root["message"] = "Token expired or uid mismatch";
        resp.result(http::status::unauthorized);
        return;
    }

    // 2. Parse body
    auto bodyStr = boost::beast::buffers_to_string(req.body().data());
    Json::Value srcRoot;
    if (Json::Reader reader; !reader.parse(bodyStr, srcRoot)) {
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        root["message"] = "Failed to parse JSON";
        resp.result(http::status::bad_request);
        return;
    }

    if (!srcRoot.isMember("upload_id")) {
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        root["message"] = "Missing upload_id";
        resp.result(http::status::bad_request);
        return;
    }

    std::string uploadId = srcRoot["upload_id"].asString();

    // 3. Query status
    auto session = UploadSessionMgr::getInstance()->get(uploadId);
    if (!session.has_value()) {
        root["error"] = static_cast<int32_t>(ErrorCodes::RESOURCE_STALE_UPLOAD);
        root["message"] = "Upload session not found or expired";
        resp.result(http::status::gone);
        return;
    }

    auto status = UploadSessionMgr::getInstance()->queryStatus(uploadId);

    root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
    root["upload_id"] = uploadId;
    root["file_name"] = session->fileName;
    root["file_size"] = static_cast<Json::Int64>(session->fileSize);
    root["chunk_size"] = session->chunkSize;
    root["total_chunks"] = session->totalChunks;
    root["received_count"] = status->receivedCount;
    root["complete"] = status->complete;

    Json::Value receivedArr(Json::arrayValue);
    for (int idx : status->receivedChunks) receivedArr.append(idx);
    root["received_chunks"] = receivedArr;

    Json::Value missingArr(Json::arrayValue);
    for (int idx : status->missingChunks) missingArr.append(idx);
    root["missing_chunks"] = missingArr;

    resp.result(http::status::ok);
}

// ============================================================
// POST /r/upload/finalize
// ============================================================
void ChunkUploadHandler::handleFinalize(std::shared_ptr<HttpConnection> conn) {
    auto& req = conn->getRequest();
    auto& resp = conn->getResponse();
    resp.set(http::field::content_type, "application/json");

    Json::Value root;
    Defer defer([&resp, &root]() {
        beast::ostream(resp.body()) << root.toStyledString();
    });

    std::cout << "Received chunk finalize request: " << conn->getRequest() << std::endl;

    // 1. Auth check
    auto auth = AuthMiddleware::authenticate(req);
    if (auth.error != 0) {
        root["error"] = auth.error;
        root["message"] = "Token expired or uid mismatch";
        resp.result(http::status::unauthorized);
        return;
    }

    // 2. Get upload_id
    auto uploadId = AuthMiddleware::extractHeaderString(req, "X-Upload-Id");
    if (uploadId.empty()) {
        root["error"] = static_cast<int32_t>(ErrorCodes::ERROR_REQUEST_JSON);
        root["message"] = "Missing header: X-Upload-Id";
        resp.result(http::status::bad_request);
        return;
    }

    // 3. Get session
    auto session = UploadSessionMgr::getInstance()->get(uploadId);
    if (!session.has_value()) {
        root["error"] = static_cast<int32_t>(ErrorCodes::RESOURCE_STALE_UPLOAD);
        root["message"] = "Upload session not found or expired";
        resp.result(http::status::gone);
        return;
    }

    // 4. Check completeness
    if (!UploadSessionMgr::getInstance()->isComplete(uploadId)) {
        auto status = UploadSessionMgr::getInstance()->queryStatus(uploadId);
        root["error"] = static_cast<int32_t>(ErrorCodes::RESOURCE_STALE_UPLOAD);
        root["message"] = "Not all chunks received";
        Json::Value missingArr(Json::arrayValue);
        for (int idx : status->missingChunks) missingArr.append(idx);
        root["missing_chunks"] = missingArr;
        resp.result(http::status::partial_content); // 206
        return;
    }

    std::string tmpDir = getTmpRootPath() + "/" + uploadId;
    std::string tmpFilePath = tmpDir + "/" + session->resourceId + ".tmp";
    std::string destDir = getUploadRootPath() + "/" + session->convId;

    // 5. Verify each chunk MD5 and merge
    {
        std::error_code ec;
        std::filesystem::create_directories(tmpDir, ec);

        std::ofstream outFile(tmpFilePath, std::ios::binary | std::ios::trunc);
        if (!outFile.is_open()) {
            root["error"] = static_cast<int32_t>(ErrorCodes::FILE_ERROR);
            root["message"] = "Failed to create merged file";
            resp.result(http::status::internal_server_error);
            return;
        }

        std::vector<char> buffer(MERGE_READ_BUFFER);
        for (int i = 0; i < session->totalChunks; ++i) {
            std::string partPath = getTmpRootPath() + "/" + uploadId + "_" + std::to_string(i) + ".part";

            // Verify chunk MD5 if recorded
            std::string expectedChunkMd5 = UploadSessionMgr::getInstance()->getChunkMd5(uploadId, i);
            if (!expectedChunkMd5.empty()) {
                std::ifstream partFile(partPath, std::ios::binary);
                if (!partFile.is_open()) {
                    root["error"] = static_cast<int32_t>(ErrorCodes::RESOURCE_MD5_MISMATCH);
                    root["message"] = "Missing chunk file: " + std::to_string(i);
                    root["corrupted_chunk"] = i;
                    resp.result(http::status::bad_request);
                    return;
                }
                Md5 chunkMd5;
                while (partFile.read(buffer.data(), buffer.size()) || partFile.gcount() > 0) {
                    chunkMd5.update(buffer.data(), partFile.gcount());
                }
                if (chunkMd5.finalize() != expectedChunkMd5) {
                    root["error"] = static_cast<int32_t>(ErrorCodes::RESOURCE_MD5_MISMATCH);
                    root["message"] = "Chunk md5 mismatch";
                    root["corrupted_chunk"] = i;
                    resp.result(http::status::bad_request);
                    return;
                }
            }

            // Append to merged file
            std::ifstream partFile(partPath, std::ios::binary);
            if (!partFile.is_open()) {
                root["error"] = static_cast<int32_t>(ErrorCodes::FILE_ERROR);
                root["message"] = "Cannot open chunk: " + std::to_string(i);
                resp.result(http::status::internal_server_error);
                return;
            }
            while (partFile.read(buffer.data(), buffer.size()) || partFile.gcount() > 0) {
                outFile.write(buffer.data(), partFile.gcount());
            }
        }
        outFile.close();
    }

    // 6. Compute overall MD5 (streaming read of merged file)
    std::string actualMd5;
    {
        Md5 md5;
        std::ifstream mergedFile(tmpFilePath, std::ios::binary);
        std::vector<char> buffer(MERGE_READ_BUFFER);
        while (mergedFile.read(buffer.data(), buffer.size()) || mergedFile.gcount() > 0) {
            md5.update(buffer.data(), mergedFile.gcount());
        }
        actualMd5 = md5.finalize();
    }

    // 7. Verify expected MD5
    if (!session->expectedMd5.empty() && actualMd5 != session->expectedMd5) {
        std::filesystem::remove(tmpFilePath);
        root["error"] = static_cast<int32_t>(ErrorCodes::RESOURCE_MD5_MISMATCH);
        root["message"] = "File md5 mismatch";
        root["file_md5"] = actualMd5;
        resp.result(http::status::bad_request);
        return;
    }

    // 8. 秒传检查 + 注册（加分布式锁防并发重复注册）
    {
        DistLockGuard md5Lock("upload_md5_" + actualMd5, 30, 10);

        ResourceMeta existingMeta;
        if (ResourceMetaMgr::getInstance()->preCheck(actualMd5, existingMeta)) {
            // 秒传命中：清理临时文件，返回已有资源
            std::error_code ec;
            std::filesystem::remove_all(getTmpRootPath() + "/" + uploadId, ec);
            for (int i = 0; i < session->totalChunks; ++i) {
                std::filesystem::remove(getTmpRootPath() + "/" + uploadId + "_" + std::to_string(i) + ".part", ec);
            }
            std::filesystem::remove(tmpFilePath, ec);
            UploadSessionMgr::getInstance()->remove(uploadId);

            root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
            root["resource_id"] = existingMeta.resourceId;
            root["url"] = "/r/" + existingMeta.resourceId;
            if (!existingMeta.thumbPath.empty()) {
                root["thumb_url"] = "/r/" + existingMeta.resourceId + "/thumb";
            }
            resp.result(http::status::ok);
            return;
        }

        // 9. Register resource
        std::error_code ec;
        std::filesystem::create_directories(destDir, ec);

        // 获取文件扩展名
        std::string ext;
        size_t dotPos = session->fileName.rfind('.');
        if (dotPos != std::string::npos) ext = session->fileName.substr(dotPos);

        std::string finalPath = destDir + "/" + session->resourceId + ext;
        std::filesystem::rename(tmpFilePath, finalPath, ec);
        if (ec) {
            std::filesystem::copy_file(tmpFilePath, finalPath, std::filesystem::copy_options::overwrite_existing, ec);
            std::filesystem::remove(tmpFilePath, ec);
        }

        ResourceMeta meta;
        meta.resourceId = session->resourceId;
        meta.convId = session->convId;
        meta.uploaderUid = auth.uid;
        meta.md5 = actualMd5;
        meta.fileSize = session->fileSize;
        meta.fileName = session->fileName;
        meta.filePath = session->convId + "/" + session->resourceId + ext;
        meta.resourceType = static_cast<uint8_t>(ResourceType::FILE);
        meta.status = static_cast<uint8_t>(ResourceStatus::NORMAL);
        meta.referenceCount = 1;

        if (!ResourceMetaMgr::getInstance()->registerResource(meta)) {
            std::filesystem::remove(finalPath, ec);
            root["error"] = static_cast<int32_t>(ErrorCodes::MYSQL_ERROR);
            root["message"] = "Failed to register resource metadata";
            resp.result(http::status::internal_server_error);
            return;
        }

        // 10. Cleanup
        for (int i = 0; i < session->totalChunks; ++i) {
            std::filesystem::remove(getTmpRootPath() + "/" + uploadId + "_" + std::to_string(i) + ".part", ec);
        }
        std::filesystem::remove_all(tmpDir, ec);
        UploadSessionMgr::getInstance()->remove(uploadId);

        root["error"] = static_cast<int32_t>(ErrorCodes::SUCCESS);
        root["resource_id"] = meta.resourceId;
        root["url"] = "/r/" + meta.resourceId;
        resp.result(http::status::ok);
    }
}
