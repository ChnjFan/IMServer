# C++ 测试框架设计方案

> 创建日期：2026-07-11  
> 状态：设计方案（待实现）  
> 适用范围：IMServer 后端 C++ 服务（GateServer / ChatServer / StatusServer / ResourceServer）

---

## 一、背景与目标

### 1.1 现状

IMServer 是一个多服务 IM 后端，包含 4 个 C++ 服务器（GateServer HTTP、ChatServer TCP+gRPC、StatusServer gRPC、ResourceServer gRPC）和 1 个 Node.js 服务（VerifyServer）。

- **ChatServer** 是核心：自定义 TCP 二进制协议（2B msgId + 2B bodyLen + JSON body）+ 有状态长连接 + gRPC 跨服务通信。
- 当前 `test/` 目录仅有一个几乎空的 CMakeLists.txt，没有实际测试体系。
- 外部依赖强：Redis（验证码/状态/锁）、MySQL（用户/好友/消息）。

### 1.2 目标

建立一套 C++ 测试框架，覆盖三个维度：

| 维度 | 关注点 | 度量 |
|------|-------|------|
| **正确性** | 功能逻辑是否正确 | 测试过/不过 |
| **稳定性** | 长跑是否有泄漏/崩溃 | fd/RSS 增长、错误率、消息丢失率 |
| **性能** | 吞吐/极限/延迟 | QPS、P50/P95/P99 延迟、崩溃点 |

### 1.3 推进策略：三阶段渐进

**选择理由**：避免"一步到位"导致框架搭一半搁置。每阶段独立可交付、独立产生价值。

| 阶段 | 名称 | 周期 | 核心产出 |
|------|------|------|---------|
| 一 | 基线正确性 | Week 1-2 | gtest 脚手架 + Gate/Status 集成测试 + CI 接入 |
| 二 | 稳定性长跑 | Week 3-4 | TestClient + 混合场景 + 度量体系 |
| 三 | 性能压测 | Week 5+ | 阶梯压测 + 基线卡 + 瓶颈分析 |

阶段二的基础设施（TestClient / Metrics / Report）被阶段三复用，不会浪费。

---

## 二、整体架构

### 2.1 分层结构

```
┌─────────────────────────────────────────────────────┐
│               测试执行层 (Test Runner)                 │
│   gtest main + 标签过滤 (unit/integration/perf)       │
├─────────────────────────────────────────────────────┤
│               场景层 (Scenarios)                       │
│   稳定性长跑 / 压力阶梯 / 性能基线采集                   │
├───────────────┬───────────────┬─────────────────────┤
│  GateServer   │ StatusServer  │   ChatServer        │
│  HTTP Client  │ gRPC Client   │  ChatTestClient     │
├───────────────┴───────────────┴─────────────────────┤
│            协议传输层 (Transport)                       │
│   HTTP(Boost.Beast) │ gRPC │ TCP 二进制协议          │
├─────────────────────────────────────────────────────┤
│            共享设施层 (Framework)                       │
│   协议编解码 │ Fixture 基类 │ 账号管理 │ Metrics │ Report│
├─────────────────────────────────────────────────────┤
│            外部依赖层 (Dependencies)                    │
│        Redis  │  MySQL  │  gRPC 服务                 │
└─────────────────────────────────────────────────────┘
```

### 2.2 测试运行形态：分层组合

| 分层 | 内容 | 频率 | 耗时 |
|------|------|------|------|
| `[unit]` | 纯逻辑、mock 外部依赖 | 每次改代码 / CI 必跑 | 秒级 |
| `[integration]` | 真连 Redis/MySQL/gRPC | PR merge 前 / nightly | 分钟级 |
| `[stability]` | 长跑场景 | 发版前 / 按需 | 小时~天级 |
| `[perf]` | 压测/基线 | 性能回归时 | 分钟~小时级 |

---

## 三、目录结构

```
test/
├── CMakeLists.txt              # 顶层：生成 IMTest 可执行文件 + 注册子目录
├── run_tests.sh                # 分层运行脚本
├── framework/
│   ├── CMakeLists.txt          # 生成 test_framework 静态库
│   ├── chat_test_client.h/cpp  # TCP 二进制协议客户端（板块重点）
│   ├── protocol.h/cpp          # 2B msgId + 2B len + body 编解码
│   ├── http_test_client.h/cpp  # GateServer HTTP 客户端
│   ├── grpc_test_client.h/cpp  # gRPC 通用客户端封装
│   ├── fixture_base.h          # IntegrationTestBase / PerfTestBase
│   ├── account_manager.h/cpp   # 测试账号分配与回收
│   ├── resource_monitor.h/cpp  # fd / RSS / 线程数采样
│   ├── metrics.h/cpp           # 延迟直方图 + 吞吐 + 错误率
│   ├── report.h/cpp            # 数据文件生成 (CSV/JSON)
│   └── stability_runner.h/cpp  # 长跑场景编排
├── gate/                       # GateServer 测试
│   ├── CMakeLists.txt
│   └── gate_integration_test.cpp
├── status/                     # StatusServer 测试
│   ├── CMakeLists.txt
│   └── status_integration_test.cpp
├── chat/                       # ChatServer 测试
│   ├── CMakeLists.txt
│   ├── protocol_test.cpp
│   └── chat_integration_test.cpp
├── integration/                # 跨服务集成测试
│   └── CMakeLists.txt
└── perf/                       # 压力/性能测试入口
    ├── CMakeLists.txt
    ├── perf_suite.h/cpp
    └── chat_perf_test.cpp
```

### CMake 组织要点

- `BUILD_TEST` 选项（默认 ON）控制是否编译测试
- `test/framework/` 编成静态库 `test_framework`，测试用例和 perf 入口链接它
- gtest 通过 `FetchContent` 引入（无需系统预装）
- 生成的 `IMTest` 放 `cmake-build-debug/bin/`

---

## 四、TestClient 与协议层（技术核心）

### 4.1 协议编解码（`protocol.h/cpp`）

帧格式与服务器一致：2B msgId + 2B bodyLen + N B JSON body。

```cpp
#pragma pack(push, 1)
struct FrameHeader {
    uint16_t msgId;
    uint16_t bodyLen;
};
#pragma pack(pop);

// 编码：msgId + Json::Value → 可直接 send 的二进制帧
std::string encode(uint16_t msgId, const Json::Value& body);

// 解码：从接收缓冲区解析，处理粘包/半包，返回完整帧列表
struct DecodedFrame { uint16_t msgId; Json::Value body; };
std::vector<DecodedFrame> decode(const char* buf, size_t len);
```

**为什么单独抽 protocol 而非内联在 Client 里？**
- 编解码逻辑有独立的单元测试需求（"我编的对不对"）
- 压测场景可能直接用 `encode` 批量组包，不走完整 Client 生命周期
- 协议层不依赖 asio，轻量可测

### 4.2 ChatTestClient 接口

```cpp
class ChatTestClient {
public:
    using MessageHandler = std::function<void(uint16_t msgId, const Json::Value& body)>;

    ChatTestClient();
    ~ChatTestClient();

    // 连接管理
    bool connect(const std::string& host, uint16_t port, int timeoutMs = 5000);
    void disconnect();
    bool isConnected() const;

    // 发送并等待匹配的回复（请求-响应模式）
    std::optional<Json::Value> sendAndWait(
        uint16_t msgId, const Json::Value& body,
        std::function<bool(uint16_t, const Json::Value&)> match,
        int timeoutMs = 5000);

    // 发送不等回复（fire-and-forget）
    bool send(uint16_t msgId, const Json::Value& body);

    // 注册某 msgId 的异步回调（通知类消息）
    void onMessage(uint16_t msgId, MessageHandler handler);

    // 业务快捷方法
    bool chatLogin(int uid, const std::string& token);
    bool sendChatMsg(int toUid, const std::string& content);
    bool heartbeat();

    // 统计
    uint64_t messagesSent() const;
    uint64_t messagesReceived() const;
    uint64_t errors() const;

private:
    void recvLoop();  // 后台收包线程，decode 后分发给 Promise 或回调
    // 成员：io_context_, socket_, recvBuffer_, 等待中的 Promise 队列, 各 handler
};
```

### 4.3 关键设计决策

| 决策点 | 选择 | 理由 |
|--------|------|------|
| IO 模型 | 单 io_context + 独立 recvLoop 线程 | 模拟真实客户端行为，比 asio async 更易写测试逻辑 |
| 发送风格 | 同步 sendAndWait(RR) + 异步 onMessage(通知) | IM 既有请求-响应也有推送，两种语义都要覆盖 |
| 错误处理 | 断开/超时抛异常 + errorCounter 累计 | 集成测试用异常断言，压测用计数器 |

### 4.4 连接与收发流程

```
connect()                              recvLoop()
   │                                       │
   ├── TCP handshake ──►                   │
   │                       recv bytes ◄───┤
   │                       ↓               │
   │                decode(缓冲区)          │
   │                 处理粘包/半包          │
   │                       ↓               │
   ├── sendAndWait() ──►  分发给           │
   │     ├── encode()    匹配的 Promise     │
   │     ├── send()      或 onMessage 回调  │
   │     └── Promise 等待应答               │
   │                                       │
```

### 4.5 协议层单元测试用例

```cpp
// ProtocolTest —— 不依赖外部服务
TEST(ProtocolTest, EncodeDecode) {
    Json::Value body; body["uid"] = 123;
    auto frame = encode(ID_CHAT_MSG_REQ, body);
    auto frames = decode(frame.data(), frame.size());
    EXPECT_EQ(frames[0].msgId, ID_CHAT_MSG_REQ);
    EXPECT_EQ(frames[0].body["uid"].asInt(), 123);
}

TEST(ProtocolTest, SplitPacket) {
    // 模拟半包：一次只给一半数据，decode 应返回空且不丢数据
}

TEST(ProtocolTest, StickyPackets) {
    // 模拟粘包：两个完整帧粘在一起，应解析出两帧
}

// ChatClientTest —— 需要 ChatServer 运行
TEST_F(ChatClientTest, LoginAndSend) {
    ASSERT_TRUE(client_.chatLogin(uid_, token_));
    auto rsp = client_.sendAndWait(ID_CHAT_MSG_REQ, makeMsg(456, "hello"),
        [](uint16_t id, const Json::Value&) { return id == ID_CHAT_MSG_RSP; });
    ASSERT_TRUE(rsp.has_value());
}
```

---

## 五、Fixture 基层与账号管理

### 5.1 Fixture 层级

```
::testing::Test                    ← gtest 基类
    └── IntegrationTestBase        ← 测试前 ping Redis/MySQL，不可用则 GTEST_SKIP()
            ├── GateIntegrationTest
            ├── StatusIntegrationTest
            └── ChatIntegrationTest

::testing::Test
    └── PerfTestBase               ← 压测基线：初始化 metrics、并发客户端池
            ├── ChatThroughputTest
            └── ChatLatencyTest
```

**为什么分两层？** `IntegrationTestBase` 统一处理"外部依赖不可用则 skip"的逻辑——开发环境 Redis 没启时不会 fail，而是 skip；CI 里则要求必须过。

```cpp
class IntegrationTestBase : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        // 检查 Redis / MySQL 连通性，不连通则标记 gSkip = true
    }
    void SetUp() override {
        if (gSkip) GTEST_SKIP() << "外部依赖不可用";
        accountMgr_ = std::make_unique<AccountManager>();
    }
    void TearDown() override {
        accountMgr_->releaseAll();
    }
    static inline bool gSkip = false;
    std::unique_ptr<AccountManager> accountMgr_;
};
```

### 5.2 AccountManager —— 测试账号隔离

核心思路：每个测试拿唯一前缀的账号，跑完自动清理，杜绝互相污染。

```cpp
struct TestAccount {
    int uid;
    std::string email;
    std::string token;
};

class AccountManager {
public:
    TestAccount acquire(const std::string& tag);
    std::vector<TestAccount> acquireBatch(int n, const std::string& tag);
    void release(int uid);          // 删 DB 记录 + 清 Redis 缓存
    void releaseAll();              // 释放本 AccountManager 持有的全部账号
private:
    std::set<int> heldUids_;
    // 账号命名: test_{tag}_{seq}@test.com，便于排查时区分来源
};
```

| 策略 | 实现 |
|------|------|
| 账号唯一 | `test_{tag}_{seq}@test.com`，tag 用测试名 |
| 数据隔离 | 所有测试数据带 `test_` 前缀或落在指定 uid 段 |
| 自清理 | `releaseAll()` 在 TearDown 中调用 |
| 可排查 | 邮箱包含测试名，即使忘了清理也能在 DB 里识别 |

---

## 六、集成测试设计（阶段一打样）

### 6.1 GateServer 测试 (GateIntegrationTest)

使用 Boost.Beast HTTP client 发请求，覆盖注册/登录/密码重置的 happy path + 错误码：

```cpp
TEST_F(GateIntegrationTest, GetVerifyCode) {
    auto rsp = http_.post("/get_verify_code", {{"email", "gate_test_1@test.com"}});
    EXPECT_EQ(rsp.code, 200);
    EXPECT_EQ(rsp.json["error"].asInt(), 0);
}

TEST_F(GateIntegrationTest, RegisterAndLogin) {
    auto acct = accountMgr_->acquire("reg");
    auto code = getVerifyCodeFromRedis(acct.email);  // 辅助函数：从 Redis 取验证码
    auto rsp = http_.post("/reg_user", makeRegBody(acct.email, "pwd123", code));
    EXPECT_EQ(rsp.json["error"].asInt(), 0);
    // 重复注册应失败
    rsp = http_.post("/reg_user", makeRegBody(acct.email, "pwd123", code));
    EXPECT_EQ(rsp.json["error"].asInt(), USER_EXISTS);
}
```

### 6.2 StatusServer 测试 (StatusIntegrationTest)

gRPC stub 调用，验证登录路由分配 ChatServer：

```cpp
TEST_F(StatusIntegrationTest, LoginQueryReturnChatServer) {
    auto stub = StatusService::NewStub(grpcChannel_);
    LoginRequest req; req.set_uid(1001);
    LoginResponse rsp; ClientContext ctx;
    auto status = stub->Login(&ctx, req, &rsp);
    EXPECT_TRUE(status.ok());
    EXPECT_FALSE(rsp.host().empty());
    EXPECT_GT(rsp.token().size(), 0);
}
```

### 6.3 测试执行节奏

```
开发者本地：
  ./IMTest --gtest_filter="*unit*"         每次改代码都跑（秒级）
  ./IMTest --gtest_filter="*integration*"   commit 前跑（秒~分钟级）

CI (PR)：
  ./IMTest 排除 integration/perf/stability    只跑 unit，不依赖外部环境

CI (merge / nightly)：
  全套，要求 Redis/MySQL 可用
```

---

## 七、度量体系、报告器与稳定性场景

### 7.1 Metrics 库（`metrics.h/cpp`）

三维覆盖：延迟分位 + 吞吐 + 错误率。

```cpp
class LatencyHistogram {
public:
    void record(int64_t us);             // 记录一次延迟（微秒）
    int64_t percentile(double p) const;  // p ∈ [0,1]
    int64_t min() const;
    int64_t max() const;
    int64_t avg() const;
    uint64_t count() const;
};

class ThroughputCounter {
public:
    void tick();                          // 完成一次操作调用一次
    uint64_t total() const;
};

class ErrorCounter {
public:
    void addFailed();
    void addSuccess();
    double errorRate() const;             // failed / (success + failed)
};

struct Metrics {
    LatencyHistogram latency;
    ThroughputCounter throughput;
    ErrorCounter errors;
    std::chrono::steady_clock::time_point startTime;
    std::string summary() const;          // 打印终端用
    Json::Value toJson() const;           // 输出数据文件用（复用项目 jsoncpp）
};
```

### 7.2 报告输出分层

| 测试类型 | 输出形式 | 用途 |
|---------|---------|------|
| `[unit]` / `[integration]` | gtest XML（`--gtest_output=xml:result.xml`） | CI 判过/不过 |
| `[stability]` / `[perf]` | 终端摘要 + 数据文件（CSV/JSON） | 人工读 + 后续可视化 |

```cpp
class ReportWriter {
public:
    void appendSample(time_t t, const Metrics& m);   // 每采样周期调用
    void writeSummary(const std::string& path);       // 终端摘要
    void writeDataFile(const std::string& path);       // CSV 时间序列
};

// CSV 格式：
// timestamp,sent,recv,failed,qps,latency_p50_us,latency_p99_us
// 1719000000,1000,998,2,33.3,1200,8500
```

### 7.3 资源监控（进程级）

稳定性测试要检测连接泄漏、内存增长、fd 泄漏。

```cpp
struct ResourceSnapshot {
    int   fdCount;       // /proc/self/fd 数量
    long  vmRSS_KB;      // /proc/self/status VmRSS
    int   threadCount;
};

class ResourceMonitor {
public:
    ResourceSnapshot sample() const;
    bool isLeaking(const ResourceSnapshot& baseline,
                   const ResourceSnapshot& current) const;
    // fd 增长 >20% || RSS 增长 >30% → 泄漏告警
};
```

### 7.4 稳定性场景（阶段二）

`StabilityRunner` 编排三种场景 + 混合模式：

```cpp
class StabilityRunner {
public:
    void runKeepAlive(int durationSec, int clientCount,
                      std::chrono::seconds heartbeatInterval);  // 长连接心跳
    void runChurn(int cycles, int clientCount,
                  std::chrono::seconds onlineSec,
                  std::chrono::seconds offlineSec);              // 反复上下线
    void runMessageStorm(int durationSec, int clientCount,
                         int msgPerClientPerSec);                // 持续消息
    void runMixed(int durationSec, int clientCount);             // A+B+C 轮转
};
```

**混合场景时序**：

```
时间轴 ─────────────────────────────────────────────►
client_1:  ══(心跳长跑)════════════════════════════►
client_2:  ▲▼▲▼▲▼▲▼▲▼▲▼▲▼▲▼▲▼▲▼▲▼▲▼▲▼▲▼▲▼▲▼▲▼   ◄── 反复上下线
client_3:  ···消息···消息···消息···消息···消息···   ◄── 持续消息
client_N:  (随机分配到以上三种行为)
```

**长跑通过/失败判据**：

| 指标 | 通过条件 | 失败标志 |
|------|---------|---------|
| 进程存活 | 全程未崩溃 | SEGFAULT / 异常退出 |
| 错误率 | < 0.1% | 连续 5min > 1% |
| fd / RSS 增长 | < 30% | 单调增长且超阈值 |
| 消息丢失 | 0 | sent != received |
| 心跳断裂 | 0 次 | 连续 3 次心跳无 PONG |

### 7.5 性能测试场景（阶段三）

```cpp
class PerfSuite {
public:
    void runRampUp();          // 阶梯施压：10→50→100→200→500 并发，每阶梯 60s
    void runToBreak();         // 极限施压：并发数×2 直到错误率 > 5%
    void runMixedWorkload();   // 读写混合：70% 消息 + 20% 好友 + 10% 登录
};
```

输出 **性能基线卡（baseline card）**：

```
┌────────────────────────────────────┐
│  Performance Baseline — ChatServer │
├────────────────────────────────────┤
│  日期      2026-07-15              │
│  机器      8C16G                   │
│  │                                 │
│  并发     QPS      P50    P99      │
│  10      3200     1.2ms   8ms      │
│  50      9800     3.1ms  25ms      │
│  100     15200    5.8ms  48ms      │
│  200     18500    11ms   120ms     │
│  500     19000    35ms   450ms     │
│  │                                 │
│  崩溃点   ≈ 800 并发               │
└────────────────────────────────────┘
```

---

## 八、三阶段落地排期

### 阶段一：基线正确性（Week 1-2）

**目标**：跑通"测试能跑、CI 能接、过不过能判"的完整链路。

| 任务 | 交付物 | 耗时 |
|------|-------|------|
| CMake + gtest 接入 + `BUILD_TEST` 开关 | 可编译出 `IMTest` 二进制 | 1d |
| `protocol.h/cpp` 编解码 + 单元测试 | `ProtocolTest.*` 全绿 | 1d |
| `account_manager.h/cpp` + `IntegrationTestBase` | 可复用 Fixture | 1d |
| GateServer HTTP client + 3 个集成测试 | `GateIntegrationTest.*` | 2d |
| StatusServer gRPC client + 2 个集成测试 | `StatusIntegrationTest.*` | 2d |
| CI 接入：PR 跑 unit、merge 跑 integration | CI 流水线脚本 | 1d |
| gtest 标签体系 + 分层运行脚本 | `run_tests.sh` | 0.5d |

**退出条件**：本地 `./IMTest` 全绿；CI 能跑。

### 阶段二：稳定性长跑（Week 3-4）

**目标**：ChatServer 能撑起 24h 长跑，发现泄漏/崩溃/消息丢失。

| 任务 | 交付物 | 耗时 |
|------|-------|------|
| `ChatTestClient` 完整实现 + 连接测试 | `ChatClientTest.*` | 3d |
| ChatServer 业务测试（登录/消息/好友） | `ChatIntegrationTest.*` | 2d |
| `StabilityRunner` + 三种场景 | 可执行的长跑入口 | 2d |
| `ResourceMonitor` + 泄漏判据 | 自动检测 fd/RSS 增长 | 1d |
| `Metrics` + `ReportWriter` + 数据文件输出 | 可画图的时序 CSV/JSON | 2d |
| 混合场景端到端验证 | 一次 ≥ 4h 长跑不出错 | 1d |

**退出条件**：24h 长跑通过判据全绿；有可复现的数据报告。

### 阶段三：性能压测与优化闭环（Week 5+）

**目标**：产出性能基线，发现瓶颈，指导优化。

| 任务 | 交付物 | 耗时 |
|------|-------|------|
| `PerfSuite` 阶梯施压 + 极限施压 | 压测入口程序 | 2d |
| baseline card 模板 + 自动输出 | 每次压测一份基线卡 | 1d |
| perf / heaptrack 集成 | 热点函数列表 + 内存 profile | 2d |
| 性能回归门禁（CI 对比基线） | 性能退化 → CI fail | 1d |
| 瓶颈定位 + 优化 → 再测闭环 | 迭代直到达标 | 持续 |

**退出条件**：有可对比的基线数据；CI 能拦截性能退化。

### 里程碑检查点

```
Week 2 末:  阶段一 review ──► Gate+Status 测试全绿, CI 已接入
Week 4 末:  阶段二 review ──► 24h 长跑通过, 有度量报告
Week 6 末:  阶段三 review ──► 性能基线卡产出, 定位 Top3 瓶颈
之后:       每发版跑稳定性 + 性能回归 → 持续守护质量
```

---

## 九、风险与对策

| 风险 | 对策 |
|------|------|
| TestClient 协议 bug 误导测试结果 | 编解码先独立单元测试打扎实；抓包对比真实客户端 |
| 长跑环境不稳定（开发机重启等） | 报告器写入文件可续传；跑在独立机器或容器 |
| 阶段二占用太多时间，阶段三被砍 | 阶段二产出（TestClient/Metrics/Report）也是阶段三共用基础设施 |
| 性能基线随机器差异不可比 | baseline card 记录机器配置；CI 跑在同一规格机器 |
| 测试账号残留污染开发 DB | `releaseAll()` 兜底 + 账号命名带 `test_` 前缀可识别清理 |

---

## 十、不在本框架范围内

以下事项**不在本次设计范围**，后续按需扩展：

- VerifyServer（Node.js）的 JavaScript 测试 → 建议用 Jest 单独建
- 前端客户端测试 → 独立的端到端测试体系
- 生产环境混沌工程 → 先跑好测试框架再考虑
- 全链路压测平台 → 当前自研 TestClient + Profiler 够用
