//
// Created by Fan on 2026/7/3.
//

#include "UploadSessionMgr.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

#include "RedisMgr.h"
#include "common/ResourceConfig.h"

std::string UploadSessionMgr::sessionKey(const std::string& id) {
    return std::string(SESSION_PREFIX) + id;
}

std::string UploadSessionMgr::chunksKey(const std::string& id) {
    return std::string(SESSION_CHUNKS_PREFIX) + id;
}

std::string UploadSessionMgr::chunkMd5Key(const std::string& id) {
    return std::string(SESSION_CHUNK_MD5_PREFIX) + id;
}

std::string UploadSessionMgr::generateUploadId() {
    static thread_local std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<uint64_t> dist;
    uint64_t value = dist(rng);

    std::stringstream ss;
    ss << "up_" << std::hex << std::setfill('0') << std::setw(12) << (value & 0xFFFFFFFFFFFFull);
    return ss.str();
}

std::optional<std::string> UploadSessionMgr::create(const std::string& convId,
                                                     const std::string& fileName,
                                                     const std::string& expectedMd5,
                                                     int64_t fileSize,
                                                     int chunkSize,
                                                     int totalChunks) {
    std::string uploadId = generateUploadId();
    std::string resourceId = generateUploadId(); // 复用随机生成器产生 resourceId 前缀不同
    // resourceId 按规范: res_ + 8位hex
    std::stringstream rss;
    rss << RESOURCE_ID_PREFIX << std::hex << std::setfill('0') << std::setw(8)
        << (std::hash<std::string>{}(uploadId) & 0xFFFFFFFF);
    resourceId = rss.str();

    auto redis = RedisMgr::getInstance();

    // 存储会话元数据
    std::unordered_map<std::string, std::string> fields;
    fields["conv_id"] = convId;
    fields["file_name"] = fileName;
    fields["expected_md5"] = expectedMd5;
    fields["resource_id"] = resourceId;
    fields["total_chunks"] = std::to_string(totalChunks);
    fields["chunk_size"] = std::to_string(chunkSize);
    fields["file_size"] = std::to_string(fileSize);
    fields["tmp_dir"] = ".upload_tmp";

    if (!redis->hSet(sessionKey(uploadId), fields)) {
        return std::nullopt;
    }

    redis->setExpire(sessionKey(uploadId), SESSION_TTL_SECONDS);
    redis->setExpire(chunksKey(uploadId), SESSION_TTL_SECONDS);
    redis->setExpire(chunkMd5Key(uploadId), SESSION_TTL_SECONDS);

    return uploadId;
}

std::optional<UploadSession> UploadSessionMgr::get(const std::string& uploadId) {
    auto redis = RedisMgr::getInstance();
    auto fields = redis->hGetAll(sessionKey(uploadId));
    if (fields.empty()) return std::nullopt;

    UploadSession session;
    session.uploadId = uploadId;
    session.convId = fields["conv_id"];
    session.fileName = fields["file_name"];
    session.expectedMd5 = fields["expected_md5"];
    session.resourceId = fields["resource_id"];
    session.totalChunks = std::stoi(fields["total_chunks"]);
    session.chunkSize = std::stoi(fields["chunk_size"]);
    session.fileSize = std::stoll(fields["file_size"]);
    session.tmpDir = fields["tmp_dir"];

    // 查询已收到的分片（chunksKey 是 Set 类型，用 SMEMBERS 读取）
    const auto receivedSet = redis->sMembers(chunksKey(uploadId));
    session.received.resize(session.totalChunks, false);
    for (const auto& idxStr : receivedSet) {
        const int idx = std::stoi(idxStr);
        if (idx >= 0 && idx < session.totalChunks) {
            session.received[idx] = true;
        }
    }

    return session;
}

bool UploadSessionMgr::markChunkReceived(const std::string& uploadId, int chunkIndex,
                                          const std::string& chunkMd5) {
    const auto redis = RedisMgr::getInstance();
    const std::string idxStr = std::to_string(chunkIndex);

    // 幂等检查
    if (redis->sIsMember(chunksKey(uploadId), idxStr)) {
        return false;
    }

    redis->sAdd(chunksKey(uploadId), idxStr);

    if (!chunkMd5.empty()) {
        redis->hSet(chunkMd5Key(uploadId), idxStr, chunkMd5);
    }

    refreshTtl(uploadId);   // 刷新数据后更新 TTL
    return true;
}

bool UploadSessionMgr::isChunkReceived(const std::string& uploadId, int chunkIndex) {
    return RedisMgr::getInstance()->sIsMember(chunksKey(uploadId), std::to_string(chunkIndex));
}

std::optional<ChunkStatus> UploadSessionMgr::queryStatus(const std::string& uploadId) {
    auto session = get(uploadId);
    if (!session.has_value()) return std::nullopt;

    ChunkStatus status;
    status.totalChunks = session->totalChunks;
    for (int i = 0; i < session->totalChunks; ++i) {
        if (session->received[i]) {
            status.receivedChunks.push_back(i);
        } else {
            status.missingChunks.push_back(i);
        }
    }
    status.receivedCount = static_cast<int>(status.receivedChunks.size());
    status.complete = status.missingChunks.empty();

    return status;
}

bool UploadSessionMgr::isComplete(const std::string& uploadId) {
    auto session = get(uploadId);
    if (!session.has_value()) return false;
    return std::all_of(session->received.begin(), session->received.end(),[](const bool recv) {
        return recv;
    });
}

std::string UploadSessionMgr::getChunkMd5(const std::string& uploadId, int chunkIndex) {
    return RedisMgr::getInstance()->hGet(chunkMd5Key(uploadId), std::to_string(chunkIndex));
}

bool UploadSessionMgr::refreshTtl(const std::string& uploadId) {
    const auto redis = RedisMgr::getInstance();
    bool ok = true;
    ok &= redis->setExpire(sessionKey(uploadId), SESSION_TTL_SECONDS);
    ok &= redis->setExpire(chunksKey(uploadId), SESSION_TTL_SECONDS);
    ok &= redis->setExpire(chunkMd5Key(uploadId), SESSION_TTL_SECONDS);
    return ok;
}

bool UploadSessionMgr::remove(const std::string& uploadId) {
    auto redis = RedisMgr::getInstance();
    bool ok = true;
    ok &= redis->del(sessionKey(uploadId));
    ok &= redis->del(chunksKey(uploadId));
    ok &= redis->del(chunkMd5Key(uploadId));
    return ok;
}

void UploadSessionMgr::scanStaleSessions(int timeoutMinutes) {
    // Redis TTL 自动过期，此方法预留用于主动扫描清理残留的 .part 文件
    // 实际实现需要遍历 .upload_tmp 目录，对比 Redis 中存活会话，删除无会话关联的 .part
    // TODO: 与 OrphanScanner 集成
    (void)timeoutMinutes;
}
