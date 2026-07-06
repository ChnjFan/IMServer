# 资源服务器设计方案

## 目录

1. [需求背景](#1-需求背景)
2. [需求汇总](#2-需求汇总)
3. [整体架构](#3-整体架构)
4. [协议与接口](#4-协议与接口)
5. [MD5 校验](#5-md5-校验)
6. [秒传与去重](#6-秒传与去重)
7. [并发安全](#7-并发安全)
8. [数据库与缓存设计](#8-数据库与缓存设计)
9. [鉴权与下载](#9-鉴权与下载)
10. [模块详细设计](#10-模块详细设计)
11. [错误码](#11-错误码)
12. [实现清单](#12-实现清单)
13. [验证方案](#13-验证方案)

---

## 1. 需求背景

### 1.1 当前现状

IMServer 已实现文本消息的收发和会话管理（见 `docs/chat_conv.md`）。

**当前缺失：**
- 文件上传与 ChatServer 耦合（旧 TCP base64 分片）
- 没有文件 MD5 **真实**校验（旧版为 FNV-1a 占位假 MD5）
- 没有断点续传能力
- 旧上传将整个文件读入内存，内存占用与文件大小线性增长

### 1.2 设计目标

1. **解耦**：ResourceServer 作为独立进程 HTTP 服务，ChatServer 对资源无感知
2. **全类型**：支持图片、文件、语音、视频等全类型富媒体
3. **安全**：Token + uid + 会话成员三重校验 + OpenSSL 真实 MD5 校验
4. **高效**：秒传去重、分片上传（内存恒定）、断点续传
5. **可扩展**：HTTP 层无状态，支持多实例水平扩展

---

## 2. 需求汇总

| 维度 | 决策 |
|------|------|
| 资源类型 | 全类型富媒体（图片、文件、语音、视频、表情包） |
| 存储后端 | 本地磁盘 `{ResourceRoot}/{conv_id}/{resource_id}.{ext}` |
| 部署形态 | 独立 ResourceServer 进程 |
| 鉴权策略 | Token + uid + 会话成员三重校验 |
| 上传协议 | **HTTP 分片上传**（默认 chunk_size = 2MB），支持断点续传 |
| MD5 计算 | **OpenSSL EVP MD5**（128 位真实 MD5，32 位 hex） |
| 图片处理 | 压缩（最长边 2048 + JPEG 85%）+ 缩略图（200×200） |
| 视频/语音 | 预留接口，本期不实现转码等高级能力 |
| 扩展性 | 多实例 + 共享存储，HTTP 层无状态 |
| 清理策略 | 异步扫描清理 orphan 资源，ChatServer 零感知 |

---

## 3. 整体架构

### 3.1 进程拓扑

```
┌─────────────────────────────────────────────────────────────────────┐
│                         客户端 (Mobile / Web)                        │
│                                                                     │
│  上传: HTTP(分片) → ResourceServer                                  │
│  发消息: TCP(聊天) → ChatServer    content = "/r/res_a1b2c3d4"       │
│  下载: HTTP GET → ResourceServer                                    │
└────────────────┬───────────────────────────────┬────────────────────┘
                 │                               │
                 ▼                               ▼
┌──────────────────────────┐        ┌───────────────────────────────┐
│     ResourceServer       │        │        ChatServer             │
│                          │        │                               │
│  ┌────────────────────┐  │        │  _sessions                    │
│  │  ResourceHttpSvc   │  │        │  _LogicSystem                 │
│  │  (HTTP 统一入口)    │  │        │  _UserMgr                     │
│  ├────────────────────┤  │        │                               │
│  │  HttpConnection    │  │        │  对资源 0 感知                  │
│  │  (Boost.Beast)     │  │        │  content 中的 URL 只是纯文本    │
│  ├────────────────────┤  │        │  原样存储、原样转发              │
│  │  ResourceLogicSys  │  │        └───────────────────────────────┘
│  │  (URL 路由分发)     │  │
│  ├────────────────────┤  │        ┌───────────────────────────────┐
│  │  AuthMiddleware    │  │        │        StatusServer            │
│  │  (三重校验)         │  │        │   (Token gRPC 校验)            │
│  ├────────────────────┤  │        └───────────────────────────────┘
│  │  ChunkUploadHandler│  │
│  │  (分片上传处理)     │  │
│  ├────────────────────┤  │
│  │  PreCheckHandler   │  │
│  │  (秒传预检)         │  │
│  ├────────────────────┤  │
│  │  DownloadHandler   │  │
│  │  (文件下载输出)     │  │
│  ├────────────────────┤  │
│  │  UploadSessionMgr  │  │
│  │  (分片会话状态)     │  │
│  ├────────────────────┤  │
│  │  ImageProcessor    │  │
│  ├────────────────────┤  │
│  │  ResourceMetaMgr   │  │
│  └────────────────────┘  │
└──────────────────────────┘
```

### 3.2 ResourceServer 内部模块

| 模块 | 职责 | 关键类 / 文件 |
|------|------|---------------|
| **ResourceHttpService** | HTTP 服务入口，accept 连接，创建 HttpConnection | `ResourceServer`（net/ResourceServer.h） |
| **HttpConnection** | 单个 HTTP 连接：读请求、路由、写响应 | `HttpConnection` + `UrlParser`（base/UrlParser.h） |
| **ResourceLogicSystem** | URL 路由分发（GET/POST handler 注册与调用） | `ResourceLogicSystem`（Singleton） |
| **AuthMiddleware** | 三重校验（Token + uid + 会话成员） | `AuthMiddleware`（service/AuthMiddleware.h） |
| **ChunkUploadHandler** | 分片上传：init/chunk/status/finalize 四端点 | `ChunkUploadHandler` |
| **PreCheckHandler** | 秒传预检：MD5 → 已存在资源直接返回 | `PreCheckHandler` |
| **DownloadHandler** | 文件流输出、缩略图、鉴权 | `DownloadHandler` |
| **UploadSessionMgr** | 分片上传会话状态（Redis Hash + Set） | `UploadSessionMgr`（Singleton） |
| **ImageProcessor** | 图片压缩、缩略图生成 | `ImageProcessor`（预留） |
| **ResourceMetaMgr** | 资源元数据 CRUD（MySQL）、缓存（Redis） | `ResourceMetaMgr` → `ResourceMetaDao` + `ResourceMetaCache` |
| **Md5** | OpenSSL EVP MD5 计算（增量 update / 一次性 compute） | `Md5`（base/Md5.h） |

### 3.3 数据流

#### 分片上传流程
```
客户端              ResourceServer
  │                       │
  │── POST /r/upload ────►│  init: 创建会话(Redis), 返回 {upload_id, chunk_size, total_chunks}
  │◄── upload_id ────────│
  │                       │
  │── POST /r/upload/chunk × N ──►  每块独立请求：
  │   X-Upload-Id                   ├─ 幂等检查（已收则跳过）
  │   X-Chunk-Index                 ├─ 写 .part 文件
  │   X-Chunk-Md5                   ├─ 分片 MD5 校验
  │                                 └─ sAdd 标记已收
  │                       │
  │══ [断点续传] ══════════│
  │── POST /r/upload/status ► 查询 received_chunks / missing_chunks
  │◄── missing_chunks ────│  只补传缺失分片
  │                       │
  │── POST /r/upload/finalize ►
  │                       ├─ 校验所有分片到齐
  │                       ├─ 逐分片 MD5 校验
  │                       ├─ 合并 .part → .tmp
  │                       ├─ 流式计算整体 MD5（64KB buffer）
  │                       ├─ 校验 expectedMd5
  │                       ├─ 秒传检查 + 分布式锁 → registerResource
  │                       ├─ rename → 正式路径
  │                       └─ 清理 .part + 删除会话
  │◄── resource_id ───────│
```

#### 下载流程
1. HTTP GET `/r/{resource_id}` → AuthMiddleware 三重校验
2. 校验通过 → 返回文件流

#### 消息流转
- 客户端拿到资源 URL 后，作为普通消息 content 通过 ChatServer 发送（TCP 聊天连接）
- ChatServer 完全不解析 content，URL 等同于纯文本

---

## 4. 协议与接口

### 4.1 上传入口 (init)

```
POST /r/upload
POST /r/upload/init
Authorization: Bearer <token>
X-User-Id: 7
Content-Type: application/json

{
  "conv_id": "c2c_3_7",
  "file_name": "photo.jpg",
  "file_md5": "d41d8cd98f00b204e9800998ecf8427e",
  "file_size": 5242880,
  "chunk_size": 2097152
}
```

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `conv_id` | string | 是 | 资源所属会话 ID |
| `file_name` | string | 是 | 原始文件名 |
| `file_md5` | string | 否 | 客户端计算的整体文件 MD5（finalize 时校验） |
| `file_size` | int64 | 是 | 文件总字节数 |
| `chunk_size` | int | 否 | 分片大小（默认 2MB，最大 8MB） |

**成功响应：**
```json
{
  "error": 0,
  "upload_id": "up_1a2b3c4d5e6f",
  "chunk_size": 2097152,
  "total_chunks": 3
}
```

### 4.2 上传分片 (chunk)

```
POST /r/upload/chunk
Authorization: Bearer <token>
X-User-Id: 7
X-Upload-Id: up_1a2b3c4d5e6f
X-Chunk-Index: 0
X-Chunk-Md5: e99a18c428cb38d5f260853678922e03

<binary chunk data>
```

| Header | 必填 | 说明 |
|--------|------|------|
| `X-Upload-Id` | 是 | init 返回的 upload_id |
| `X-Chunk-Index` | 是 | 分片序号（0-based） |
| `X-Chunk-Md5` | 否 | 该分片的 MD5（服务端写入后校验） |

**成功响应：**
```json
{ "error": 0, "status": "ok", "received": 1, "total": 3 }
```

**幂等响应（分片已收到）：**
```json
{ "error": 0, "status": "already_exists", "received": 1, "total": 3 }
```

### 4.3 断点续传状态查询 (status)

```
POST /r/upload/status
Authorization: Bearer <token>
X-User-Id: 7
Content-Type: application/json

{ "upload_id": "up_1a2b3c4d5e6f" }
```

**响应：**
```json
{
  "error": 0,
  "upload_id": "up_1a2b3c4d5e6f",
  "file_name": "photo.jpg",
  "file_size": 5242880,
  "chunk_size": 2097152,
  "total_chunks": 3,
  "received_chunks": [0, 1],
  "missing_chunks": [2],
  "received_count": 2,
  "complete": false
}
```

客户端根据 `missing_chunks` 只补传缺失分片。

### 4.4 完成上传 (finalize)

```
POST /r/upload/finalize
Authorization: Bearer <token>
X-User-Id: 7
X-Upload-Id: up_1a2b3c4d5e6f
```

**成功响应：**
```json
{
  "error": 0,
  "resource_id": "res_a1b2c3d4",
  "url": "/r/res_a1b2c3d4",
  "thumb_url": "/r/res_a1b2c3d4/thumb"
}
```

**分片未到齐（206）：**
```json
{
  "error": 5007,
  "message": "Not all chunks received",
  "missing_chunks": [2]
}
```

**秒传命中：**
```json
{
  "error": 0,
  "resource_id": "res_existing",
  "url": "/r/res_existing"
}
```

### 4.5 预检接口 (秒传)

```
POST /r/check
Authorization: Bearer <token>
X-User-Id: 7
Content-Type: application/json

{ "file_md5": "d41d8cd98f00b204e9800998ecf8427e" }
```

**响应（资源已存在 — 秒传成功）：**
```json
{
  "found": true,
  "resource_id": "res_a1b2c3d4",
  "url": "/r/res_a1b2c3d4",
  "thumb_url": "/r/res_a1b2c3d4/thumb"
}
```

**响应（资源不存在）：**
```json
{ "found": false }
```

### 4.6 下载协议 (HTTP)

```
GET /r/{resource_id}               → 原图/原文件
GET /r/{resource_id}/thumb          → 缩略图（仅图片类型）
```

**请求头：**
```
Authorization: Bearer <token>
X-User-Id: 7
X-Conversation-Id: c2c_3_7
```

**成功响应头：**
```
HTTP/1.1 200 OK
Content-Type: image/jpeg
Content-Length: 5242880
Accept-Ranges: bytes
Cache-Control: private, max-age=86400
ETag: "res_a1b2c3d4_d41d8cd98f00b204e9800998ecf8427e"
```

---

## 5. MD5 校验

### 5.1 实现

使用 **OpenSSL EVP API** 实现真实 MD5（128 位），封装于 `src/base/Md5.h`。

```cpp
class Md5 {
public:
    Md5();                                 // EVP_MD_CTX_new + EVP_DigestInit_ex(md5)
    void update(const void* data, size_t len);  // 增量喂数据
    std::string finalize();                 // 返回 32 位小写 hex
    static std::string compute(const void* data, size_t len);  // 一次性计算
    static bool verify(const void* data, size_t len, const std::string& expectedHex);
};
```

### 5.2 校验层次

| 阶段 | 校验内容 | 时机 |
|------|----------|------|
| **分片 MD5** | 客户端上报 `X-Chunk-Md5`，服务端对写入的 `.part` 文件计算 MD5 比对 | 每个 chunk 请求时 |
| **整体 MD5** | finalize 时对合并后的完整文件流式计算 MD5（64KB buffer），与 `file_md5` 比对 | finalize 时 |
| **finalize 分片复核** | finalize 时重新读取每个 `.part`，与 chunk 上传时记录的 MD5 比对，发现网络/磁盘损坏分片 | finalize 时 |

### 5.3 内存效率

finalize 合并后计算整体 MD5 时，使用 64KB buffer 分块读取合并文件（`Md5::update`），**不将整个文件载入内存**，内存占用恒定。

---

## 6. 秒传与去重

### 6.1 秒传流程

客户端在上传前先调用 `POST /r/check` 预检接口：
1. 客户端计算本地文件 MD5
2. 发送预检请求 `{ file_md5 }`
3. 服务端查询 Redis 缓存（`resource_md5_{md5}`），命中则直接返回已有资源 URL
4. 未命中则查 MySQL `resource_meta` 表（`idx_md5` 索引）
5. 找到 → 缓存到 Redis 并返回秒传结果；未找到 → 返回 `found:false`
6. 客户端根据 `found` 决定是否发起分片上传

### 6.2 去重存储策略

物理文件只存一份，通过不同记录指向同一 `file_path` 实现跨会话去重：

| 场景 | 处理 |
|------|------|
| 同会话同文件再次上传 | 秒传：返回已有 resource_id |
| 不同会话上传相同文件 | 秒传检查命中 → 新增一条 `resource_meta` 记录指向同一 `file_path` |
| 消息被删除 | 异步 scanner 检测引用归零 → 清理物理文件 |
| 用户注销/会话删除 | 遍历关联资源，按引用计数判断是否删物理文件 |

### 6.3 resource_id 生成规则

```cpp
// 格式: "res_" + 8位随机hex = 总长度12字符
// 例: "res_a1b2c3d4"
// 16^8 = 4,294,967,296 种组合，杜绝递增遍历
```

upload_id 格式：`"up_" + 12位随机hex`。

---

## 7. 并发安全

### 7.1 威胁与对策

| 场景 | 安全？ | 说明 |
|------|--------|------|
| 不同文件并发上传 | ✅ 安全 | 不同 upload_id、不同 .part 文件、不同 resourceId，无共享状态 |
| 同一文件并发 finalize | ⚠️ 需加锁 | 多个客户端都通过 preCheck → 都 registerResource → 重复记录 |
| 同一分片并发写入 | ✅ 安全 | `isChunkReceived` 前置检查 + Redis `sAdd` 原子幂等，第二个请求直接返回 `already_exists` |

### 7.2 分布式锁保护注册临界区

复用 base/DistLock / DistLockGuard（Redis SETNX 实现），以 **MD5 作锁 key** 包住 preCheck → registerResource：

```cpp
{
    DistLockGuard md5Lock("upload_md5_" + actualMd5, /*timeout=*/30, /*acquire=*/10);

    // double-check 秒传
    if (preCheck(actualMd5, existingMeta)) { 清理分片; 返回秒传; }

    registerResource(meta);
}
```

| 参数 | 值 | 理由 |
|------|-----|------|
| 锁 key | `upload_md5_{md5}` | 同一文件（同 MD5）互斥，不同文件不阻塞 |
| timeout（持锁超时） | 30s | 注册操作通常 < 1s，留足余量防死锁 |
| acquireTimeout（等待获取） | 10s | 等待另一线程注册完成；超时返回 409 让客户端重试 |

### 7.3 失败路径资源清理

- 分片 MD5 校验失败 → 删除该 `.part`
- finalize 注册失败 → 保留 `.part`（可重试），超时会话由 `scanStaleSessions` 清理
- 客户端放弃 → 30 分钟后 Redis TTL 过期 + 残留 `.part` 由定时任务清理

---

## 8. 数据库与缓存设计

### 8.1 资源元数据表 `resource_meta`

```sql
CREATE TABLE `resource_meta` (
  `resource_id`    varchar(32) NOT NULL COMMENT '资源唯一ID，格式: res_{8位随机hex}',
  `conv_id`        varchar(64) NOT NULL COMMENT '来源会话ID',
  `uploader_uid`   int NOT NULL COMMENT '上传者用户ID',
  `md5`            char(32) NOT NULL COMMENT '文件MD5（小写32位hex）',
  `file_size`      bigint NOT NULL DEFAULT 0 COMMENT '文件大小(字节)',
  `file_name`      varchar(255) NOT NULL COMMENT '原始文件名',
  `file_path`      varchar(512) NOT NULL COMMENT '物理存储相对路径',
  `thumb_path`     varchar(512) DEFAULT NULL COMMENT '缩略图相对路径',
  `resource_type`  tinyint NOT NULL COMMENT '1:图片 2:文件 3:语音 4:视频 5:其他',
  `status`         tinyint NOT NULL DEFAULT 0 COMMENT '0:正常 1:上传中 2:已删除',
  `reference_count` int NOT NULL DEFAULT 1 COMMENT '引用计数',
  `width`          int DEFAULT NULL COMMENT '图片/视频宽度(px)',
  `height`         int DEFAULT NULL COMMENT '图片/视频高度(px)',
  `duration`       int DEFAULT NULL COMMENT '音视频时长(ms)，暂不使用',
  `create_time`    datetime(3) DEFAULT CURRENT_TIMESTAMP(3),
  `update_time`    datetime(3) DEFAULT CURRENT_TIMESTAMP(3) ON UPDATE CURRENT_TIMESTAMP(3),
  PRIMARY KEY (`resource_id`),
  KEY `idx_md5` (`md5`) COMMENT '秒传查询',
  KEY `idx_conv_id` (`conv_id`) COMMENT '会话资源列表',
  KEY `idx_uploader` (`uploader_uid`) COMMENT '上传者查询'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='资源元数据表';
```

### 8.2 Redis Key 设计

| Key 模式 | 类型 | 用途 | TTL |
|----------|------|------|-----|
| `resource_md5_{md5}` | String | MD5 → resource_id（秒传索引） | 无 TTL，写穿透 |
| `resource_meta_{resource_id}` | Hash | 资源元数据缓存 | 无 TTL，写穿透 |
| `upload_session_{id}` | Hash | 分片上传会话元数据 | 30 分钟 |
| `upload_chunks_{id}` | Set | 已收到的分片索引 | 30 分钟 |
| `upload_chunk_md5_{id}` | Hash | 分片索引 → 分片 MD5 | 30 分钟 |

### 8.3 DAO 层接口

```cpp
class ResourceMetaDao {
public:
    bool insert(const ResourceMeta& meta);
    std::optional<ResourceMeta> selectByResourceId(const std::string& resourceId);
    std::optional<ResourceMeta> selectByMd5(const std::string& md5);
    std::vector<ResourceMeta> selectByConvId(const std::string& convId, int limit, int offset);
    bool updateRefCount(const std::string& resourceId, int delta);
    bool remove(const std::string& resourceId);
};
```

### 8.4 C++ 数据结构

```cpp
enum class ResourceType : uint8_t {
    IMAGE = 1, FILE = 2, VOICE = 3, VIDEO = 4, OTHER = 5,
};

enum class ResourceStatus : uint8_t {
    NORMAL = 0, UPLOADING = 1, DELETED = 2,
};

struct ResourceMeta {
    std::string resourceId;
    std::string convId;
    int         uploaderUid = -1;
    std::string md5;
    int64_t     fileSize = 0;
    std::string fileName;
    std::string filePath;
    std::string thumbPath;
    uint8_t     resourceType = 0;
    uint8_t     status = 0;
    int         referenceCount = -1;
    int         width = -1, height = -1, duration = -1;
    std::string createTime;
};
```

### 8.5 存储路径规则

```
{ResourceRoot}/{conv_id}/{resource_id}.{orig_ext}          ← 原图/原文件
{ResourceRoot}/{conv_id}/{resource_id}_thumb.jpg           ← 缩略图
{ResourceRoot}/.upload_tmp/{upload_id}_{index}.part        ← 分片临时文件
{ResourceRoot}/.upload_tmp/{upload_id}/{resourceId}.tmp   ← 合并临时文件
```

其中 `{ResourceRoot}` 来自 `config.ini` 的 `[ResourceServer] Path`。

---

## 9. 鉴权与下载

### 9.1 三重校验流程

```
客户端                        ResourceServer              StatusServer
  │                               │                           │
  │── GET /r/{resource_id} ──────►│                           │
  │   Authorization: Bearer tok   │                           │
  │   X-User-Id: 7                │                           │
  │   X-Conversation-Id: c2c_3_7  │                           │
  │                               │                           │
  │                               │── gRPC VerifyToken ───────►│
  │                               │◄── { uid=7, valid } ──────│
  │                               │                           │
  │                               │── uid(header) == uid(tok)? │
  │                               │── uid 是 conv_id 成员?     │
  │◄── 200 + 文件流 ──────────────│                           │
```

### 9.2 校验层次

| 校验层 | 校验内容 | 失败码 |
|--------|----------|--------|
| **请求头完整性** | token、X-User-Id、X-Conversation-Id 均存在 | 5001 |
| **Token 合法性 + uid 一致性** | Token 有效且解析出的 uid == X-User-Id | 5001 |
| **会话成员权限** | uid 是 conv_id 的参与方 | 5002 |

### 9.3 AuthMiddleware 实现

```cpp
class AuthMiddleware {
public:
    struct AuthResult { int error = 0; int uid = -1; std::string convId; };

    static AuthResult authenticate(const http::request<http::dynamic_body>& req);

    // 供 ChunkUploadHandler 读取自定义 header
    static int extractHeaderInt(const http::request<http::dynamic_body>& req,
                                const std::string& name);
    static std::string extractHeaderString(const http::request<http::dynamic_body>& req,
                                           const std::string& name);
};
```

### 9.4 DownloadHandler

文件流输出、Range 支持（预留）、缩略图、MIME 类型猜测。当前实现通过 `boost::beast::file_posix` 读取文件。

---

## 10. 模块详细设计

### 10.1 ResourceServer 进程结构

```
ResourceServer (main.cpp)
├── ResourceHttpService       (HTTP 服务，统一入口)
│   ├── HttpConnection        (单个 HTTP 连接，Boost.Beast)
│   └── ResourceLogicSystem   (URL 路由分发)
├── AuthMiddleware            (三重校验)
├── ChunkUploadHandler        (分片上传: init/chunk/status/finalize)
├── PreCheckHandler           (秒传预检)
├── DownloadHandler           (文件下载输出)
├── UploadSessionMgr          (分片会话 Redis 状态)
├── ImageProcessor            (图片处理，线程池异步 — 预留)
├── ResourceMetaMgr           (元数据管理)
│   ├── ResourceMetaDao       (MySQL)
│   └── ResourceMetaCache     (Redis)
└── Config                    (ConfigMgr + ImageProcessConfig)
```

### 10.2 路由注册

```cpp
// ResourceLogicSystem.cpp
registerPost("/r/upload",          ChunkUploadHandler::handleInit);     // 上传入口
registerPost("/r/upload/init",     ChunkUploadHandler::handleInit);     // 同上（别名）
registerPost("/r/upload/chunk",    ChunkUploadHandler::handleChunk);    // 上传分片
registerPost("/r/upload/status",   ChunkUploadHandler::handleStatus);   // 断点续传查询
registerPost("/r/upload/finalize", ChunkUploadHandler::handleFinalize); // 完成上传
registerPost("/r/check",           PreCheckHandler::handle);            // 秒传预检
registerGet("/r/",                 DownloadHandler::handle);            // 下载
```

### 10.3 与 ChatServer 的交互

**设计决策：零交互。**

- 客户端拿到 resource_url 后作为普通消息 content 发送
- ChatServer 不解析 content，URL 等同于纯文本
- 消息删除时无需通知 ResourceServer
- 资源清理由 Orphan Scanner 异步兜底

旧 ChatServer 的 `LogicWorker::fileUploadHandler()` 分片上传逻辑已被废弃（客户端改为直接 HTTP 请求 ResourceServer）。

---

## 11. 错误码

| 码 | 定义 | 场景 |
|----|------|------|
| 5001 | RESOURCE_AUTH_FAILED | Token 无效/过期/uid 不匹配 |
| 5002 | RESOURCE_ACCESS_DENIED | 非会话成员 |
| 5003 | RESOURCE_NOT_FOUND | resource_id 不存在 |
| 5004 | RESOURCE_MD5_MISMATCH | 文件 MD5 校验失败 |
| 5005 | RESOURCE_FORMAT_INVALID | 文件名格式非法 |
| 5006 | RESOURCE_SIZE_EXCEEDED | 文件超过大小限制 |
| 5007 | RESOURCE_STALE_UPLOAD | 上传会话超时 |

---

## 12. 实现清单

### 12.1 base 库新增

| 文件 | 说明 | 状态 |
|------|------|------|
| `src/base/Md5.h` / `Md5.cpp` | OpenSSL EVP MD5 RAII 封装 | ✅ 已完成 |
| `src/base/UrlParser.h` / `UrlParser.cpp` | URL 解析器（从 HttpConnection 提取） | ✅ 已完成 |
| `src/base/DistLock.h` / `DistLock.cpp` | Redis 分布式锁（已存在，复用） | ✅ 已有 |

### 12.2 ResourceServer 服务层

| 文件 | 说明 | 状态 |
|------|------|------|
| `src/ResourceServer/net/ResourceServer.h/.cpp` | HTTP 服务入口 | ✅ 已有 |
| `src/ResourceServer/net/HttpConnection.h/.cpp` | HTTP 连接 + UrlParser | ✅ 已有 |
| `src/ResourceServer/core/ResourceLogicSystem.h/.cp`| URL 路由分发 | ✅ 已有 |
| `src/ResourceServer/service/AuthMiddleware.h/.cpp` | 三重校验 | ✅ 已有 |
| `src/ResourceServer/service/ChunkUploadHandler.h/.cpp` | **分片上传四端点** | ✅ 已完成 |
| `src/ResourceServer/service/PreCheckHandler.h/.cpp` | 秒传预检 | ✅ 已有 |
| `src/ResourceServer/service/DownloadHandler.h/.cpp` | 文件下载 | ⚠️ 编译错误待修复 |
| `src/ResourceServer/service/UploadSessionMgr.h/.cpp` | **分片会话 Redis 管理** | ✅ 已完成 |
| `src/ResourceServer/service/UploadHandler.h/.cp`| 旧单请求上传（已废弃路由） | ⚠️ 保留代码但不再注册 |
| `src/ResourceServer/common/model/ResourceMeta.h` | 数据模型 | ✅ 已有 |
| `src/ResourceServer/core/ResourceMetaMgr.h/.cpp` | 元数据管理 | ✅ 已有 |
| `src/ResourceServer/db/mysql/dao/ResourceMetaDao.h/.cpp` | MySQL DAO | ✅ 已有 |
| `src/ResourceServer/db/redis/ResourceMetaCache.h/.cpp` | Redis 缓存 | ✅ 已有 |
| `src/ResourceServer/common/ResourceConfig.h` | 配置常量 + ImageProcessConfig | ✅ 已有 |

### 12.3 配置

```ini
[ResourceServer]
Name = ResourceServer
Host = 127.0.0.1
Port = 50058              ; HTTP 端口（上传/下载/预检共用）
Path = resources          ; 文件存储根目录

[ImageProcess]
MaxDimension = 2048       ; 图片最长边限制
Quality = 85              ; JPEG 压缩质量(0-100)
ThumbSize = 200           ; 缩略图尺寸(宽高)
MaxFileSize = 52428800    ; 最大单文件 50MB
```

---

## 13. 验证方案

### 13.1 编译验证

```bash
cd cmake-build-debug && ninja
```

### 13.2 端到端场景 (curl)

```bash
# 1. 秒传预检
curl -X POST http://localhost:50058/r/check \
  -H "Authorization: Bearer $TOKEN" -H "X-User-Id: 7" \
  -H "Content-Type: application/json" \
  -d '{"file_md5":"..."}'
# → { "found": true/false }

# 2. 分片上传 init
curl -X POST http://localhost:50058/r/upload \
  -H "Authorization: Bearer $TOKEN" -H "X-User-Id: 7" \
  -H "Content-Type: application/json" \
  -d '{"conv_id":"c2c_3_7","file_name":"test.jpg","file_md5":"'"$(md5q test.jpg)"'","file_size":'"$(stat -f%z test.jpg)"',"chunk_size":1048576}'
# → { "upload_id": "up_xxx", "chunk_size": 1048576, "total_chunks": N }

# 3. 逐片上传
UPLOAD_ID="up_XXXX"
for i in $(seq 0 $((N-1))); do
  dd if=test.jpg bs=1048576 skip=$i count=1 2>/dev/null | \
    curl -X POST http://localhost:50058/r/upload/chunk \
      -H "Authorization: Bearer $TOKEN" -H "X-User-Id: 7" \
      -H "X-Upload-Id: $UPLOAD_ID" -H "X-Chunk-Index: $i" \
      -H "X-Chunk-Md5: $(dd if=test.jpg bs=1048576 skip=$i count=1 2>/dev/null | md5)" \
      --data-binary @-
done

# 4. 断点续传（查询缺失分片）
curl -X POST http://localhost:50058/r/upload/status \
  -H "Authorization: Bearer $TOKEN" -H "X-User-Id: 7" \
  -H "Content-Type: application/json" \
  -d '{"upload_id":"up_XXXX"}'
# → { "received_chunks": [0,1], "missing_chunks": [2], ... }
# 只补传 missing_chunks

# 5. finalize
curl -X POST http://localhost:50058/r/upload/finalize \
  -H "Authorization: Bearer $TOKEN" -H "X-User-Id: 7" \
  -H "X-Upload-Id: $UPLOAD_ID"
# → { "resource_id": "res_xxx", "url": "/r/res_xxx" }

# 6. 下载
curl -H "Authorization: Bearer $TOKEN" -H "X-User-Id: 7" \
  http://localhost:50058/r/res_xxx -o downloaded.jpg
```

### 13.3 并发安全验证

10 个并发上传同一文件（同 MD5），验证只产生一条 `resource_meta` 记录：
```bash
for i in $(seq 1 10); do
  # 独立 init → 分片上传 → finalize (后台并行)
done
# SELECT COUNT(*) FROM resource_meta WHERE md5='...' = 1
```

### 13.4 大文件内存观察

```bash
dd if=/dev/urandom of=/tmp/big.bin bs=1M count=200  # 200MB
# 分片上传时观察 ResourceServer RSS，应稳定在 ~10MB 级别
```
