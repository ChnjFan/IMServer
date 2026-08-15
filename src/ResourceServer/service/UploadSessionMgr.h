//
// Created by Fan on 2026/7/3.
//

#ifndef IMSERVER_UPLOADSESSIONMGR_H
#define IMSERVER_UPLOADSESSIONMGR_H

#include <optional>
#include <string>
#include <vector>

#include "Singleton.h"

struct UploadSession {
    std::string uploadId;
    std::string convId;
    std::string fileName;
    std::string expectedMd5;
    std::string resourceId;
    int totalChunks = 0;
    int chunkSize = 0;
    int64_t fileSize = 0;
    std::string tmpDir;
    std::vector<bool> received;
};

struct ChunkStatus {
    int totalChunks = 0;
    int receivedCount = 0;
    std::vector<int> receivedChunks;
    std::vector<int> missingChunks;
    bool complete = false;
};

class UploadSessionMgr : public Singleton<UploadSessionMgr> {
    friend class Singleton<UploadSessionMgr>;
public:
    ~UploadSessionMgr() = default;

    // 创建会话，返回 upload_id（格式: "up_" + 12位随机hex）
    std::optional<std::string> create(const std::string& convId, const std::string& fileName,
                                       const std::string& expectedMd5, int64_t fileSize,
                                       int chunkSize, int totalChunks);

    // 查询会话
    std::optional<UploadSession> get(const std::string& uploadId);

    // 标记某分片已收到（幂等：重复调用返回 false 表示已存在）
    bool markChunkReceived(const std::string& uploadId, int chunkIndex,
                           const std::string& chunkMd5 = "");

    // 查询某分片是否已收到
    bool isChunkReceived(const std::string& uploadId, int chunkIndex);

    // 查询断点续传状态
    std::optional<ChunkStatus> queryStatus(const std::string& uploadId);

    // 检查是否全部收齐
    bool isComplete(const std::string& uploadId);

    // 获取某分片已记录的 MD5
    std::string getChunkMd5(const std::string& uploadId, int chunkIndex);

    // 客户端心跳保活，重置 TTL
    bool refreshTtl(const std::string& uploadId);

    // 删除会话
    bool remove(const std::string& uploadId);

    // 清理超时会话
    void scanStaleSessions(int timeoutMinutes = 30);

private:
    UploadSessionMgr() = default;

    static constexpr const char* SESSION_PREFIX = "upload_session_";
    static constexpr const char* SESSION_CHUNKS_PREFIX = "upload_chunks_";
    static constexpr const char* SESSION_CHUNK_MD5_PREFIX = "upload_chunk_md5_";
    static constexpr int SESSION_TTL_SECONDS = 1800; // 30 分钟

    static std::string sessionKey(const std::string& id);
    static std::string chunksKey(const std::string& id);
    static std::string chunkMd5Key(const std::string& id);
    static std::string generateUploadId();
};

#endif //IMSERVER_UPLOADSESSIONMGR_H
