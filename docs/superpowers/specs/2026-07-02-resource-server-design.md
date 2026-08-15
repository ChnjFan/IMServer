# IM 资源服务器设计方案

## 目录

1. [需求背景](#1-需求背景)
2. [需求汇总](#2-需求汇总)
3. [整体架构](#3-整体架构)
4. [协议与接口](#4-协议与接口)
5. [秒传与去重](#5-秒传与去重)
6. [数据库与缓存设计](#6-数据库与缓存设计)
7. [图片处理](#7-图片处理)
8. [鉴权与下载](#8-鉴权与下载)
9. [模块详细设计](#9-模块详细设计)
10. [架构优化：ChatServer 解耦与孤儿资源回收](#10-架构优化chatserver-解耦与孤儿资源回收)
11. [性能与安全](#11-性能与安全)
12. [错误码](#12-错误码)
13. [实现清单](#13-实现清单)
14. [验证方案](#14-验证方案)

---

## 1. 需求背景

### 1.1 当前现状

IMServer 已实现文本消息的收发和会话管理（见 `docs/chat_conv.md`）。ChatServer 中的 `LogicWorker::fileUploadHandler()` 已有一个 TCP 分片 base64 上传的实现，文件保存在 `static/{conv_id}/` 目录下，消息表 `msg_type=2` 预留了图片/文件类型。

**当前缺失：**
- 没有独立的资源服务器，文件上传与 ChatServer 耦合
- 文件上传后无法通过 HTTP 下载/访问
- 没有文件 MD5 校验、秒传、去重
- 没有图片压缩/缩略图能力
- 没有下载鉴权（任何知道路径的人都能访问）

### 1.2 设计目标

1. **解耦**：ResourceServer 作为独立进程，ChatServer 对资源无感知
2. **全类型**：支持图片、文件、语音、视频等全类型富媒体（本期实现上传/下载框架，语音视频具体处理预留）
3. **安全**：Token + uid + 会话成员三重校验
4. **高效**：秒传去重、图片压缩缩略图、异步处理
5. **可扩展**：HTTP 下载无状态，支持多实例水平扩展

---

## 2. 需求汇总

| 维度 | 决策 |
|------|------|
| 资源类型 | 全类型富媒体（图片、文件、语音、视频、表情包） |
| 存储后端 | 本地磁盘（多实例时共享 NAS） |
| 部署形态 | 独立 ResourceServer 进程 |
| 鉴权策略 | Token + uid + 会话成员三重校验 |
| 图片处理 | 压缩（最长边 2048 + JPEG 85%）+ 缩略图（200×200） |
| 上传协议 | HTTP multipart/form-data（单请求完成，无需手动分片） |
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
│  上传: HTTP(multipart/form-data) → ResourceServer                   │
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
│  │  (统一 HTTP 入口)   │  │        │  _UserMgr                     │
│  ├────────────────────┤  │        │                               │
│  │  HttpConnection    │  │        │  对资源 0 感知                  │
│  │  (Boost.Beast)     │  │        │  content 中的 URL 只是纯文本    │
│  ├────────────────────┤  │        │  原样存储、原样转发              │
│  │  ResourceLogicSys  │  │        │                               │
│  │  (URL 路由分发)     │  │        └───────────────────────────────┘
│  ├────────────────────┤  │
│  │  AuthMiddleware    │  │        ┌───────────────────────────────┐
│  │  (三重校验)         │  │        │        StatusServer            │
│  ├────────────────────┤  │        │   (Token gRPC 校验)            │
│  │  UploadHandler     │  │        └───────────────────────────────┘
│  │  (multipart 解析)   │  │
│  ├────────────────────┤  │
│  │  DownloadHandler   │  │
│  │  (文件流输出)       │  │
│  ├────────────────────┤  │
│  │  ImageProcessor    │  │
│  ├────────────────────┤  │
│  │  ResourceMetaMgr   │  │
│  ├────────────────────┤  │
│  │  OrphanScanner     │  │
│  └────────────────────┘  │
└──────────────────────────┘
```

### 3.2 ResourceServer 内部模块

| 模块 | 职责 | 关键类 |
|------|------|--------|
| **ResourceHttpService** | HTTP 服务入口，accept 连接，创建 HttpConnection | `ResourceHttpService` |
| **HttpConnection** | 单个 HTTP 连接：读请求、路由、写响应 | `HttpConnection`, `UrlParser` |
| **ResourceLogicSystem** | URL 路由分发（GET/POST handler 注册与调用） | `ResourceLogicSystem` |
| **AuthMiddleware** | 三重校验（Token + uid + 会话成员） | `AuthMiddleware` |
| **UploadHandler** | multipart/form-data 解析、文件写入、MD5 校验 | `UploadHandler` |
| **DownloadHandler** | 文件流输出、Range 支持、缩略图 | `DownloadHandler` |
| **ImageProcessor** | 图片压缩、缩略图生成（stb_image 系列） | `ImageProcessor` |
| **ResourceMetaMgr** | 资源元数据 CRUD（MySQL）、缓存（Redis） | `ResourceMetaDao`, `ResourceMetaCache` |
| **OrphanScanner** | 定时扫描孤儿资源并清理 | `OrphanScanner` |
| **StatusGrpcClient** | gRPC 连接池，调用 StatusServer 的 VerifyToken | 复用 `ServiceConnPool<StatusService>` |

### 3.3 数据流

#### 上传流程
1. HTTP POST `/r/upload`（multipart/form-data）→ 服务端接收完整 body
2. 解析 multipart → 提取 file 二进制 + conv_id + file_md5
3. 写入临时文件 → MD5 校验 → rename 临时为正式
4. 提交图片处理（压缩+缩略图，异步线程池）
5. 写入 MySQL 元数据 + Redis 缓存
6. 返回 `resource_id` + `url`（第 4-6 步不阻塞客户端发消息）

#### 下载流程
1. HTTP GET `/r/{resource_id}` → AuthMiddleware 三重校验
2. 校验通过 → 返回文件流（支持 Range 断点续传）

#### 消息流转
- 客户端拿到资源 URL 后，作为普通消息 content 通过 ChatServer 发送（TCP 聊天连接）
- ChatServer 完全不解析 content，URL 等同于纯文本

---

## 4. 协议与接口

ResourceServer 统一使用 HTTP 协议通信，所有端点复用同一个 HTTP 端口。

### 4.1 上传协议（HTTP multipart/form-data）

使用标准 HTTP `multipart/form-data` 格式上传文件，单请求完成上传，无需手动分片。

```
POST /r/upload
Authorization: Bearer <token>
X-User-Id: 7
Content-Type: multipart/form-data; boundary=----WebKitFormBoundary7MA4YWxkTrZu0gW

------WebKitFormBoundary7MA4YWxkTrZu0gW
Content-Disposition: form-data; name="file"; filename="photo.jpg"
Content-Type: image/jpeg

<binary data>
------WebKitFormBoundary7MA4YWxkTrZu0gW
Content-Disposition: form-data; name="conv_id"

c2c_3_7
------WebKitFormBoundary7MA4YWxkTrZu0gW
Content-Disposition: form-data; name="file_md5"

d41d8cd98f00b204e9800998ecf8427e
------WebKitFormBoundary7MA4YWxkTrZu0gW--
```

**multipart 字段说明：**

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `file` | File | 是 | 文件二进制内容 |
| `conv_id` | Text | 是 | 资源所属会话 ID |
| `file_md5` | Text | 否 | 客户端计算的 MD5（用于秒传二次校验） |

**请求头：**

| 头 | 必填 | 说明 |
|----|------|------|
| `Authorization` | 是 | `Bearer <token>`，登录 token |
| `X-User-Id` | 是 | 上传方 uid（显式传递） |
| `Content-Type` | 是 | `multipart/form-data; boundary=...` |

**成功响应：**

```json
{
  "error": 0,
  "resource_id": "res_a1b2c3d4",
  "url": "/r/res_a1b2c3d4",
  "thumb_url": "/r/res_a1b2c3d4/thumb"
}
```

**错误响应：**

```json
// 400 JSON 解析失败
{ "error": 1001, "message": "Invalid request" }

// 401 Token 无效/过期/uid 不匹配
{ "error": 5001, "message": "Token expired or uid mismatch" }

// 400 MD5 校验失败
{ "error": 5004, "message": "File md5 mismatch" }

// 400 文件超过大小限制
{ "error": 5006, "message": "File size exceeds limit (50MB)" }

// 400 文件名格式非法
{ "error": 5005, "message": "Invalid file name" }
```

**与旧 TCP 协议的关键差异：**

| 维度 | 旧 TCP 分片 | 新 HTTP multipart |
|------|-------------|-------------------|
| 传输层 | 自定义 TCP | HTTP/1.1 |
| 数据编码 | base64（膨胀 ~33%） | 原始二进制（无膨胀） |
| 分片逻辑 | 客户端手动分片（seq/total_size/trans_size） | HTTP 层自动处理，单请求完成 |
| 消息头 | 4 字节二进制头（msgId + bodySize） | 标准 HTTP 请求头 |
| 并发模型 | 单连接独占写入 | HTTP 天然并发，每请求独立 |
| 客户端复杂度 | 需实现分片/重传逻辑 | 标准 HTTP 库原生支持 |

### 4.2 预检接口（HTTP，秒传）

```
POST /r/check
Authorization: Bearer <token>
Content-Type: application/json

{
  "file_md5": "d41d8cd98f00b204e9800998ecf8427e",
  "file_size": 5242880,
  "name": "photo.jpg"
}
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

### 4.3 下载协议（HTTP）

```
GET /r/{resource_id}               → 原图/原文件
GET /r/{resource_id}/thumb          → 缩略图（仅图片类型）
GET /r/{resource_id}?w=200&h=200   → 指定尺寸（预留）
```

**请求头：**

```
Authorization: Bearer <token>          // 登录 token
X-User-Id: 7                           // 下载方 uid（显式传递）
X-Conversation-Id: c2c_3_7             // 资源所属会话
```

**成功响应头：**

```
HTTP/1.1 200 OK
Content-Type: image/jpeg
Content-Length: 5242880
Content-MD5: 1B2M2Y8AsgTpgAmY7PhCfg==
Accept-Ranges: bytes
Cache-Control: private, max-age=86400
ETag: "res_a1b2c3d4_d41d8cd98f00b204e9800998ecf8427e"
```

**错误响应：**

```json
// 401 未认证
{ "error": 5001, "message": "Token expired or uid mismatch" }

// 403 非会话成员
{ "error": 5002, "message": "Not a conversation member" }

// 404 资源不存在
{ "error": 5003, "message": "Resource not found" }
```

### 4.4 鉴权通信（gRPC 调用 StatusServer）

ResourceServer 不对外暴露 gRPC 服务。Token 校验通过调用已有的 `StatusService.VerifyToken` 完成：

```
ResourceServer ── gRPC ──► StatusServer
               VerifyTokenReq { uid, token }
               VerifyTokenRsp { error, uid }
```

**gRPC 连接池：** 复用 `ServiceConnPool<StatusService>`（定义于 `src/proto/ServiceConnPool.h`），通过 `StatusGrpcClient` 单例（`src/proto/StatusGrpcClient.h`）调用。

> 二期如需 ResourceServer 主动通知 ChatServer（如资源删除），再考虑定义 `resource.proto`。

---

## 5. 秒传与去重

### 5.1 秒传流程

客户端在上传前先调用 `POST /r/check` 预检接口：
1. 客户端计算本地文件 MD5
2. 发送预检请求 `{ file_md5, file_size, name }`
3. 服务端查询 Redis 缓存（`resource_md5:{md5}`），命中则直接返回已有资源 URL（毫秒级响应）
4. 未命中则查 MySQL `resource_meta` 表（`idx_md5` 索引）
5. 找到 → 缓存到 Redis 并返回秒传结果；未找到 → 返回 `found:false`
6. 客户端根据 `found` 决定是否发起 HTTP 实际上传

### 5.2 去重存储策略

物理文件只存一份，通过不同记录指向同一 `file_path` 实现跨会话去重：

| 场景 | 处理 |
|------|------|
| 同会话同文件再次上传 | 秒传：返回已有 resource_id |
| 不同会话上传相同文件（如转发图片） | 秒传成功 + 新增一条 `resource_meta` 记录指向同一 `file_path`（不同 conv_id） |
| 消息被删除 | 异步 scanner 检测引用归零 → 清理物理文件 |
| 用户注销/会话删除 | 遍历关联资源，按引用计数判断是否删物理文件 |

### 5.3 resource_id 生成规则

```cpp
// 格式: "res_" + 8位随机hex = 总长度12字符
// 例: "res_a1b2c3d4"
// 16^8 = 4,294,967,296 种组合，杜绝递增遍历
std::string generateResourceId() {
    uint32_t value = secureRandomUint32();
    return "res_" + toHex8(value);
}
```

---

## 6. 数据库与缓存设计

### 6.1 资源元数据表 `resource_meta`

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

### 6.2 Redis Key 设计

| Key 模式 | 类型 | 用途 | TTL |
|----------|------|------|-----|
| `resource_md5:{md5}` | String | MD5 → resource_id（秒传索引） | 无 TTL，写穿透 |
| `resource_meta:{resource_id}` | Hash | 资源元数据缓存 | 无 TTL，写穿透 |
| `conv_resources:{conv_id}` | Sorted Set | 会话资源列表（score=create_time） | 无 TTL |

### 6.3 DAO 层接口

```cpp
class ResourceMetaDao {
public:
    bool insert(const ResourceMeta& meta);
    std::optional<ResourceMeta> selectByResourceId(const std::string& resourceId);
    std::optional<ResourceMeta> selectByMd5(const std::string& md5);
    std::vector<ResourceMeta> selectByConvId(const std::string& convId, int limit, int offset);
    bool updateRefCount(const std::string& resourceId, int delta);
    bool remove(const std::string& resourceId);
    // Orphan Scanner 专用
    std::vector<ResourceMeta> selectZeroRefDeleted();
    std::vector<ResourceMeta> selectPotentiallyOrphan();
    std::vector<ResourceMeta> selectStaleUploading(int minutes);
};
```

### 6.4 C++ 数据结构

```cpp
enum class ResourceType : uint8_t {
    IMAGE = 1,
    FILE  = 2,
    VOICE = 3,
    VIDEO = 4,
    OTHER = 5,
};

enum class ResourceStatus : uint8_t {
    NORMAL    = 0,
    UPLOADING = 1,
    DELETED   = 2,
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
    int         referenceCount = 0;
    int         width = 0;
    int         height = 0;
    int         duration = 0;
    std::string createTime;

    void fromJson(Json::Value& value);
    void toJson(Json::Value& value) const;
};
```

### 6.5 存储路径规则

```
{ResourceRoot}/{conv_id}/{resource_id}.{orig_ext}          ← 原图/原文件
{ResourceRoot}/{conv_id}/{resource_id}_thumb.jpg           ← 缩略图
```

其中 `{ResourceRoot}` 来自 `config.ini` 的 `[ResourceServer] Path`。

---

## 7. 图片处理

### 7.1 处理流水线

```
原文件写入完成
      │
      ▼
┌───────────────────┐
│  判断 resource_type │
└───────┬───────────┘
        │
   type=1 (图片)?
   是      否 → 跳过，直接注册元数据
   │
   ▼
┌────────────────────┐
│ 1. probe 图片信息   │  ← stb_image
│    (width,height,  │
│     channels)      │
└───────┬────────────┘
        │
        ▼
┌────────────────────────┐
│ 2. 压缩原图             │  ← stb_image_resize
│    最长边 > 2048?       │     Lanczos 采样
│    等比缩放 + JPEG 85%  │
└───────┬────────────────┘
        │
        ▼
┌────────────────────────┐
│ 3. 生成缩略图           │  ← stb_image_resize
│    200×200             │     等比缩放 + 居中裁剪
└───────┬────────────────┘
        │
        ▼
    更新 meta + cache
```

### 7.2 配置参数

```ini
[ResourceServer]
Name = ResourceServer
Host = 127.0.0.1
Port = 50058              ; HTTP 端口（统一：上传/下载/预检共用）
Path = resources          ; 文件存储根目录
UploadTimeout = 30        ; 上传超时(分钟)

[ImageProcess]
MaxDimension = 2048       ; 图片最长边限制
Quality = 85              ; JPEG 压缩质量(0-100)
ThumbSize = 200           ; 缩略图尺寸(宽高)
EnableCompress = true     ; 是否启用压缩
EnableThumb = true        ; 是否生成缩略图
MaxFileSize = 52428800    ; 最大单文件 50MB
```

### 7.3 ImageProcessor 接口

```cpp
struct ImageInfo {
    int width = 0, height = 0, channels = 0;
    std::string format;
};

struct ProcessResult {
    std::string compressedPath;
    std::string thumbPath;
    int finalWidth = 0, finalHeight = 0;
    int64_t compressedSize = 0;
    bool compressed = false;
};

class ImageProcessor {
public:
    static std::optional<ImageInfo> probe(const std::string& filePath);
    static std::expected<ProcessResult, int> process(
        const std::string& srcPath,
        const std::string& destDir,
        const std::string& resourceId
    );
private:
    static bool compress(const std::string& src, const std::string& dst,
                         int maxDim, int quality);
    static bool makeThumbnail(const std::string& src, const std::string& dst,
                              int thumbSize);
};
```

### 7.4 选型：stb_image 系列

| 库 | 用途 | 引入方式 |
|----|------|----------|
| `stb_image.h` | 解码 JPEG/PNG/WebP/BMP/GIF | CMake FetchContent |
| `stb_image_resize2.h` | 高质量缩放（Lanczos） | 同上 |
| `stb_image_write.h` | 编码 JPEG/PNG 输出 | 同上 |

**仅头文件（header-only）**，零外部依赖。

### 7.5 上传时序（含图片处理）

```
客户端              ResourceServer (HTTP 线程)     线程池
  │                       │                         │
  │── POST /r/upload ───►│                         │
  │   multipart/form-data │                         │
  │   (完整文件二进制)     │                         │
  │                       │── 解析 multipart         │
  │                       │── 写入临时文件           │
  │                       │── MD5 校验              │
  │                       │── rename 临时→正式       │
  │                       │── 提交图片处理 ──────────►│
  │◄── 200 resource_id ──│                         │── 压缩
  │   (客户端可立即发消息) │                         │── 缩略图
  │                       │◄── 回调 ────────────────│
  │                       │── 更新 meta + cache     │
```

**关键设计：** 图片处理在独立线程池异步执行，不阻塞上传。客户端拿到 resource_id 后可立即发送消息，缩略图就绪后客户端刷新。

---

## 8. 鉴权与下载

### 8.1 三重校验流程

```
客户端                        ResourceServer              StatusServer
  │                               │                           │
  │── GET /r/{resource_id} ──────►│                           │
  │   Authorization: Bearer tok   │                           │
  │   X-User-Id: 7                │                           │
  │   X-Conversation-Id: c2c_3_7  │                           │
  │                               │                           │
  │                               │── 1. 提取请求头            │
  │                               │   token, uid, conv_id      │
  │                               │                           │
  │                               │── 2. gRPC VerifyToken ────►│
  │                               │◄── { uid=7, valid } ──────│
  │                               │                           │
  │                               │── 3. 校验 uid(header) ==   │
  │                               │   uid(token)               │
  │                               │                           │
  │                               │── 4. 查资源元数据           │
  │                               │   得到 conv_id             │
  │                               │                           │
  │                               │── 5. 校验 uid 是            │
  │                               │   conv_id 的成员           │
  │                               │                           │
  │◄── 6. 200 + 文件流 ───────────│                           │
```

### 8.2 校验层次

| 校验层 | 校验内容 | 失败码 | 说明 |
|--------|----------|--------|------|
| **第一重：请求头完整性** | token、X-User-Id、X-Conversation-Id 均存在 | 5001 | 防止缺少必要参数 |
| **第二重：Token 合法性 + uid 一致性** | Token 有效且解析出的 uid == X-User-Id | 5001 | 防止 token 被盗用后冒充他人 |
| **第三重：会话成员权限** | uid 是 conv_id 的参与方（从 conv_id 解析） | 5002 | 防止会话外成员访问 |

### 8.3 AuthMiddleware 实现

```cpp
class AuthMiddleware {
public:
    struct AuthResult {
        int error = 0;
        int uid = -1;
        std::string token;
        std::string convId;
    };
    
    AuthResult authenticate(const http_request& req) {
        auto token    = extractBearerToken(req);
        auto uid      = extractHeaderUid(req);
        auto convId   = extractHeaderConvId(req);
        
        if (token.empty() || uid < 0 || convId.empty())
            return { RESOURCE_AUTH_FAILED };
        
        auto verifyResult = grpc::VerifyToken(token);
        if (!verifyResult.valid || verifyResult.uid != uid)
            return { RESOURCE_AUTH_FAILED };
        
        return { 0, uid, token, convId };
    }
};
```

### 8.4 DownloadHandler 实现

```cpp
class DownloadHandler {
public:
    void handleRequest(const http_request& req, http_response& resp) {
        auto auth = auth_.authenticate(req);
        if (auth.error != 0) { resp = makeError(auth.error); return; }
        
        auto resourceId = extractResourceId(req);
        auto meta = ResourceMetaMgr::getInstance().getResource(resourceId);
        if (!meta.has_value()) { resp = makeError(RESOURCE_NOT_FOUND); return; }
        
        if (!isConversationMember(auth.uid, meta->convId, auth.convId)) {
            resp = makeError(RESOURCE_ACCESS_DENIED);
            return;
        }
        
        bool wantThumb = req.target().ends_with("/thumb");
        std::string path = wantThumb ? meta->thumbPath : meta->filePath;
        
        resp.set(field::content_type, guessMimeType(meta->fileName));
        resp.set("Content-MD5", base64_encode(md5Binary(meta->md5)));
        resp.set("Accept-Ranges", "bytes");
        resp.set("Cache-Control", "private, max-age=86400");
        
        if (req.count(field::range))
            serveRange(resp, path, req);
        else
            serveFull(resp, path);
    }
};
```

---

## 9. 模块详细设计

### 9.1 ResourceServer 进程结构

```
ResourceServer (main.cpp)
├── ResourceHttpService       (HTTP 服务，统一入口)
│   ├── HttpConnection        (单个 HTTP 连接，Boost.Beast)
│   └── ResourceLogicSystem   (URL 路由分发)
├── AuthMiddleware            (三重校验)
├── UploadHandler            (multipart 解析 + 文件写入)
├── DownloadHandler          (文件流输出 + Range)
├── ImageProcessor            (图片处理，线程池异步)
├── ResourceMetaMgr           (元数据管理)
│   ├── ResourceMetaDao       (MySQL)
│   └── ResourceMetaCache     (Redis)
├── OrphanScanner            (异步扫描清理)
└── StatusGrpcClient         (gRPC 调用 StatusServer 鉴权)
```

### 9.2 类关系

```
                    ┌─────────────────────┐
                    │  ResourceHttpService │
                    │  _acceptor           │
                    └──────────┬──────────┘
                               │
                    ┌──────────┴──────────┐
                    │   HttpConnection     │
                    │   request_/response_ │
                    └──────────┬──────────┘
                               │
                    ┌──────────┴──────────┐
                    │ ResourceLogicSystem  │
                    │  _getHandlers        │
                    │  _postHandlers       │
                    └──┬───────┬───────┬──┘
                       │       │       │
              ┌────────┘       │       └────────┐
              ▼                ▼                ▼
        ┌───────────┐  ┌────────────┐  ┌──────────────┐
        │Upload     │  │PreCheck    │  │Download      │
        │Handler    │  │Handler     │  │Handler       │
        └─────┬─────┘  └────────────┘  └──────┬───────┘
              │                               │
    ┌─────────┴──────────┐           ┌────────┴────────┐
    ▼                    ▼           ▼                 ▼
┌──────────┐  ┌──────────────┐  ┌──────────┐  ┌──────────────┐
│ImageProc │  │ResourceMetaMg│  │AuthMdlwr │  │ResourceMetaMg│
│(线程池)   │  │              │  │(gRPC)    │  │              │
└──────────┘  └──────────────┘  └──────────┘  └──────────────┘
```

### 9.3 核心类定义

```cpp
// ResourceHttpService.h — HTTP 服务入口
class ResourceHttpService : public std::enable_shared_from_this<ResourceHttpService> {
public:
    ResourceHttpService(net::io_context& ioc, uint16_t port);
    void start();
private:
    void acceptLoop();
    tcp::acceptor acceptor_;
    net::io_context& ioCtx_;
};

// HttpConnection.h — 单个 HTTP 连接（复用 GateServer 模式）
class HttpConnection : public std::enable_shared_from_this<HttpConnection> {
public:
    typedef std::unordered_map<std::string, std::string> UrlParams;
    friend class ResourceLogicSystem;

    explicit HttpConnection(net::io_context& io_context);
    void start();
    tcp::socket& getSocket() { return socket_; }

    class UrlParser {
    public:
        void parse(const std::string& url);
        const std::string& getPath() const;
        const UrlParams& getParams() const;
        bool hasParam(const std::string& key) const;
        std::string getParam(const std::string& key, const std::string& defaultValue = "") const;
    private:
        std::string path_;
        UrlParams params_;
        static std::string urlDecode(const std::string& encoded);
    };

private:
    void checkDeadline();
    void writeResponse();
    void handleRequest();

    tcp::socket socket_;
    beast::flat_buffer buffer_{8192};
    http::request<http::dynamic_body> request_;
    http::response<http::dynamic_body> response_;
    net::steady_timer deadline_{socket_.get_executor(), std::chrono::seconds(60)};
    UrlParser urlParser_;
};

// ResourceLogicSystem.h — URL 路由分发（复用 GateServer 模式）
class ResourceLogicSystem : public Singleton<ResourceLogicSystem> {
public:
    ~ResourceLogicSystem();
    bool handleGet(const std::string& path, std::shared_ptr<HttpConnection> conn);
    bool handlePost(const std::string& path, std::shared_ptr<HttpConnection> conn);
    void registerGet(const std::string& path, HttpRequestCallback cb);
    void registerPost(const std::string& path, HttpRequestCallback cb);
private:
    friend class Singleton<ResourceLogicSystem>;
    LogicSystem();
    std::unordered_map<std::string, HttpRequestCallback> postHandlers_;
    std::unordered_map<std::string, HttpRequestCallback> getHandlers_;
};

// AuthMiddleware.h — 三重校验
class AuthMiddleware {
public:
    struct AuthResult {
        int error = 0;
        int uid = -1;
        std::string convId;
    };
    AuthResult authenticate(const HttpConnection& conn);
};

// UploadHandler.h — multipart 上传处理
class UploadHandler {
public:
    void handle(std::shared_ptr<HttpConnection> conn);
private:
    bool parseMultipart(const http::request<http::dynamic_body>& req,
                        std::string& fileName, std::string& convId,
                        std::string& fileMd5, std::vector<uint8_t>& fileData);
    bool saveToDisk(const std::vector<uint8_t>& data, const std::string& path);
    std::string computeMd5(const std::vector<uint8_t>& data);
    std::string generateResourceId();
};

// DownloadHandler.h — 文件下载
class DownloadHandler {
public:
    void handle(std::shared_ptr<HttpConnection> conn);
private:
    void serveFull(std::shared_ptr<HttpConnection> conn, const std::string& path,
                   const ResourceMeta& meta);
    void serveRange(std::shared_ptr<HttpConnection> conn, const std::string& path,
                    const http::request<http::dynamic_body>& req);
    bool checkPermission(int uid, const std::string& convId, const ResourceMeta& meta);
};

// ResourceMetaMgr.h — 元数据管理（保持不变）
class ResourceMetaMgr {
public:
    static ResourceMetaMgr& getInstance();
    bool preCheck(const std::string& md5, ResourceMeta& meta);
    bool registerResource(const ResourceMeta& meta);
    bool getResource(const std::string& resourceId, ResourceMeta& meta);
    bool acquire(const std::string& resourceId);
    bool release(const std::string& resourceId);
    bool updateThumbPath(const std::string& resourceId, const std::string& thumbPath);
private:
    ResourceMetaMgr() = default;
    ResourceMetaDao dao_;
    ResourceMetaCache cache_;
};
```

### 9.4 HTTP 路由注册

```cpp
// ResourceLogicSystem.cpp 构造函数中注册路由
ResourceLogicSystem::ResourceLogicSystem() {
    // 上传
    registerPost("/r/upload", [](std::shared_ptr<HttpConnection> conn) {
        UploadHandler handler;
        handler.handle(conn);
    });

    // 秒传预检
    registerPost("/r/check", [](std::shared_ptr<HttpConnection> conn) {
        PreCheckHandler handler;
        handler.handle(conn);
    });

    // 下载（含缩略图）
    registerGet("/r/", [](std::shared_ptr<HttpConnection> conn) {
        DownloadHandler handler;
        handler.handle(conn);
    });
}
```

### 9.5 与 ChatServer 的交互

**设计决策：零交互。**

- 客户端拿到 resource_url 后作为普通消息 content 发送
- ChatServer 不解析 content，URL 等同于纯文本
- 消息删除时无需通知 ResourceServer
- 资源清理由 Orphan Scanner 异步兜底

> 注：全 HTTP 后，ChatServer 不再需要 `fileUploadHandler`（原 TCP 分片上传逻辑），客户端改为直接 HTTP 请求 ResourceServer。

---

## 10. 架构优化：ChatServer 解耦与孤儿资源回收

### 10.1 ChatServer 零感知

ChatServer 的聊天消息处理完全不需要改动。消息 content 中出现 `/r/xxx` 这样的 URL 和出现一段普通文本对 ChatServer 来说没有区别：

```cpp
// ChatLogicSystem::chatMsgHandle — 不需要任何修改
// content 字段可以是 "你好" 也可以是 "/r/res_a1b2c3d4"
// ChatServer 只做 Store and Forward，不解析 content
```

### 10.2 客户端职责

客户端是唯一需要理解资源 URL 的一方：

```
发送流程:                          接收流程:
1. HTTP POST /r/upload              1. 收到聊天消息，解析 content
   multipart/form-data              2. 识别出 /r/ 前缀的 URL
   上传文件到 ResourceServer         3. HTTP GET 下载资源
2. 拿到 resource_id + url           4. 渲染到聊天界面
3. 构造聊天消息:
   content = url
   content_type = 2 (图片/文件)
4. 通过 ChatServer 发送 (TCP)
```

### 10.3 OrphanScanner 设计

```cpp
class OrphanScanner {
public:
    void start(std::chrono::minutes interval = std::chrono::minutes(60));
    void stop();
    
private:
    struct ScanResult {
        std::vector<std::string> orphanIds;
        std::vector<std::string> expiredIds;
    };
    
    ScanResult scan();
    void cleanup(const ScanResult& targets);
    void scanStaleUploads(std::chrono::minutes timeout = std::chrono::minutes(30));
    
    std::atomic<bool> running_{false};
    std::thread scanThread_;
    std::chrono::minutes interval_;
};
```

### 10.4 扫描策略

```cpp
OrphanScanner::ScanResult OrphanScanner::scan() {
    ScanResult result;
    
    // 策略 1: reference_count == 0 且 status=DELETED
    auto candidates = ResourceMetaDao::selectZeroRefDeleted();
    for (auto& meta : candidates)
        result.orphanIds.push_back(meta.resourceId);
    
    // 策略 2: reference_count > 0 但 message 表无引用（防御性兜底）
    auto staleRefs = ResourceMetaDao::selectPotentiallyOrphan();
    for (auto& meta : staleRefs) {
        bool referenced = checkMessageContainsUrl(meta.resourceId);
        if (!referenced) result.orphanIds.push_back(meta.resourceId);
    }
    
    // 策略 3: status=UPLOADING 超时未完成的僵死上传
    auto staleUploads = ResourceMetaDao::selectStaleUploading(30);
    for (auto& meta : staleUploads)
        result.expiredIds.push_back(meta.resourceId);
    
    return result;
}
```

### 10.5 清理策略

```cpp
void OrphanScanner::cleanup(const ScanResult& targets) {
    for (const auto& id : targets.orphanIds) {
        auto meta = ResourceMetaMgr::getInstance().getResource(id);
        if (!meta.has_value()) continue;
        
        // 1. 删除物理文件
        std::filesystem::remove(meta->filePath);
        if (!meta->thumbPath.empty())
            std::filesystem::remove(meta->thumbPath);
        
        // 2. 删除空目录
        auto dir = std::filesystem::path(meta->filePath).parent_path();
        if (std::filesystem::is_empty(dir))
            std::filesystem::remove(dir);
        
        // 3. 删除 MySQL 记录 + Redis 缓存
        ResourceMetaDao::remove(id);
        ResourceMetaCache::remove(id);
    }
    
    // 僵死上传：标记 DELETED，引用归零，下一轮清理
    for (const auto& id : targets.expiredIds) {
        ResourceMetaDao::updateStatus(id, ResourceStatus::DELETED);
        ResourceMetaDao::updateRefCount(id, -1);
    }
}
```

### 10.6 完整交互时序

```
发送方               ChatServer           ResourceServer          接收方
   │                     │                     │                    │
   │── HTTP POST /r/upload ──────────────────►│                    │
   │   multipart/form-data                    │                    │
   │   (file + conv_id + file_md5)            │                    │
   │◄── 200 resource_id ──────────────────────│                    │
   │   url=/r/res_abc                         │                    │
   │                     │                     │                    │
   │── TCP 聊天消息 ─────►│                     │                    │
   │  content=/r/res_abc  │                     │                    │
   │  content_type=2      │── 存储 message      │                    │
   │                     │── 推送 ──────────────────────────────────►│
   │                     │                     │                    │
   │                     │                     │◄── GET /r/res_abc ──│
   │                     │                     │    X-User-Id: 7     │
   │                     │                     │    X-Conversation-Id│
   │                     │                     │── 三重校验 ✓        │
   │                     │                     │── 文件流 ────────────►│
```

---

## 11. 性能与安全

### 11.1 性能设计

| 维度 | 策略 |
|------|------|
| **上传** | HTTP multipart/form-data，单请求完成，无需手动分片 |
| **下载** | HTTP Range 支持断点续传；热点资源 Redis 缓存元数据 |
| **图片处理** | 独立线程池异步处理，不阻塞上传主流程 |
| **秒传** | MD5 → Redis 缓存命中时 0 磁盘 IO，直接返回 |
| **连接管理** | IO 多线程 + 工作线程池，复用 AsioIOServicePool 模式 |
| **热点文件** | 二期可前置 Nginx proxy_cache，ResourceServer 本身无状态 |

### 11.2 图片处理上限

| 参数 | 值 | 说明 |
|------|----|------|
| 原图最长边 | 2048px | 超过则等比缩小 |
| 压缩质量 | 85% | JPEG 平衡点 |
| 缩略图 | 200×200 | 等比缩放 + 居中裁剪 |
| 最大单文件 | 50MB | 超过拒绝上传 |
| 支持的格式 | JPEG, PNG, WebP, BMP, GIF(首帧) | 其他格式当普通文件处理 |

### 11.3 安全设计

| 威胁 | 防护 |
|------|------|
| **未授权访问** | Token + uid + 会话成员三重校验 |
| **Token 盗用** | X-User-Id 与 Token 绑定校验 |
| **路径遍历** | resource_id 仅允许 `[a-z0-9]{8}` 格式 |
| **暴力枚举** | resource_id 使用随机 hex，无递增规律 |
| **恶意文件上传** | 校验 magic byte 与扩展名一致（通过后缀白名单 + 文件头双重校验） |
| **存储溢出** | 单用户存储配额 + 总量监控（二期） |
| **HTTPS** | 生产环境前置 Nginx 终止 TLS |
| **数据残留** | Orphan Scanner 兜底清理，不依赖 ChatServer 通知 |

---

## 12. 错误码

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

## 13. 实现清单

### 13.1 新增文件

| 文件 | 说明 |
|------|------|
| `src/ResourceServer/net/ResourceHttpService.h/.cpp` | HTTP 服务入口（替代 TCP ResourceServer） |
| `src/ResourceServer/net/HttpConnection.h/.cpp` | HTTP 连接（复用 GateServer 模式） |
| `src/ResourceServer/net/ResourceLogicSystem.h/.cpp` | URL 路由分发 |
| `src/ResourceServer/service/UploadHandler.h/.cpp` | multipart 上传处理 |
| `src/ResourceServer/service/DownloadHandler.h/.cpp` | 文件下载 |
| `src/ResourceServer/service/PreCheckHandler.h/.cpp` | 秒传预检 |
| `src/ResourceServer/service/AuthMiddleware.h/.cpp` | 鉴权中间件 |
| `src/ResourceServer/service/ImageProcessor.h/.cpp` | 图片处理 |
| `src/ResourceServer/service/OrphanScanner.h/.cpp` | 孤儿资源扫描 |
| `scripts/resource.sql` | 表结构 |

### 13.2 删除/废弃文件

| 文件 | 说明 |
|------|------|
| `src/ResourceServer/net/ResourceServer.h/.cpp` | TCP acceptor，替换为 ResourceHttpService |
| `src/ResourceServer/net/Session.h/.cpp` | TCP 会话，不再需要 |
| `src/ResourceServer/net/MsgNode.h/.cpp` | TCP 消息节点，不再需要 |

### 13.3 修改文件

| 文件 | 变更内容 |
|------|----------|
| `src/ResourceServer/main.cpp` | 启动 HTTP 服务而非 TCP |
| `src/ResourceServer/CMakeLists.txt` | 更新源文件列表（移除 TCP 文件，新增 HTTP 文件） |
| `config.ini` | `[ResourceServer]` 统一为单一 `Port = 50058`，移除 `RPCPort` |
| `src/base/const.h` | 新增错误码 5001-5007（已存在）；移除 `ID_CHAT_UPLOAD_FILE_REQ/RSP`（确认 ChatServer 不再引用后） |
| `src/ChatServer/core/LogicWorker.cpp` | 移除 `fileUploadHandler`（客户端改为 HTTP 上传） |
| `src/ChatServer/core/ChatLogicSystem.cpp` | 移除 `ID_CHAT_UPLOAD_FILE_REQ` 的消息转发 |

### 13.4 保留不变的文件

| 文件 | 说明 |
|------|------|
| `src/ResourceServer/core/ResourceMetaMgr.h/.cpp` | 元数据管理 |
| `src/ResourceServer/common/model/ResourceMeta.h` | 数据模型 |
| `src/ResourceServer/common/ResourceConfig.h` | 配置常量 |
| `src/ResourceServer/db/mysql/dao/ResourceMetaDao.h/.cpp` | MySQL DAO |
| `src/ResourceServer/db/mysql/MysqlMgr.h/.cpp` | MySQL 管理器 |
| `src/ResourceServer/db/redis/ResourceMetaCache.h/.cpp` | Redis 缓存 |

### 13.5 依赖引入

| 依赖 | 用途 | 引入方式 |
|------|------|----------|
| stb_image.h | 图片解码 | CMake FetchContent |
| stb_image_resize2.h | 图片缩放 | CMake FetchContent |
| stb_image_write.h | 图片编码 | CMake FetchContent |

---

## 14. 验证方案

### 14.1 编译验证

```bash
cd cmake-build-debug && ninja
# 期望: ResourceServer 二进制生成于 bin/ResourceServer
```

### 14.2 数据库验证

```sql
-- 1. 确认表结构
DESC resource_meta;

-- 2. 插入测试记录
INSERT INTO resource_meta (resource_id, conv_id, uploader_uid, md5, file_size, file_name, file_path, resource_type)
VALUES ('res_test0001', 'c2c_3_7', 3, 'd41d8cd98f00b204e9800998ecf8427e', 1024, 'test.jpg', 'c2c_3_7/res_test0001.jpg', 1);

-- 3. 秒传查询
SELECT * FROM resource_meta WHERE md5 = 'd41d8cd98f00b204e9800998ecf8427e';
```

### 14.3 端到端场景

| 步骤 | 操作 | 验证点 |
|------|------|--------|
| 1 | 客户端 POST /r/check (md5 不存在) | 返回 `found:false` |
| 2 | 客户端 HTTP POST /r/upload (multipart) | 服务端写入磁盘，返回 resource_id |
| 3 | 客户端 POST /r/check (md5 已存在) | 返回 `found:true` + URL（秒传） |
| 4 | 客户端发送聊天消息 content=url | ChatServer 正常存储转发 |
| 5 | 接收方 HTTP GET 下载 | 三重校验通过，返回文件流 |
| 6 | 接收方 HTTP GET (错误 token) | 返回 401 |
| 7 | 非会话成员 HTTP GET | 返回 403 |
| 8 | 等待 Orphan Scanner 执行 | 无引用资源被清理 |

### 14.4 图片处理验证

| 步骤 | 操作 | 验证点 |
|------|------|--------|
| 1 | 上传 4000×3000 的大图 | 压缩为 2048×1536，质量 85% |
| 2 | 上传 100×100 小图 | 不压缩（未超阈值） |
| 3 | 下载缩略图 | 返回 200×200 居中裁剪 |
| 4 | 上传伪装扩展名的文件 | 校验 magic byte 拒绝 |
