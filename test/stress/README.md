# IMServer 压力测试

单机 10K 连接压力测试模块，使用 gtest + Boost 框架。

## 文件结构

```
test/stress/
├── stress_test_client.h/.cpp     # 单个 TCP 长连接客户端
├── stress_connection_pool.h/.cpp # 连接池 (多 io_context)
├── stress_metrics.h/.cpp         # 无锁指标采集
├── stress_fixture.h/.cpp         # 测试夹具 (账号预注册 12000)
├── scenario_burst.cpp            # 场景1: 瞬时连接风暴 (100/1K/5K/10K)
├── scenario_ramp.cpp             # 场景2: 阶梯加压 (500→10K)
├── scenario_sustained.cpp        # 场景3: 持续负载 (1K/5K/10K × 10min)
├── scenario_mixed.cpp            # 场景4: 混合场景 (500基础 + churn)
├── scenario_throughput.cpp       # 场景5: 消息吞吐探测 (纯聊天吞吐饱和)
├── scenario_mixed_throughput.cpp # 场景6: 混合消息吞吐探测 (聊天+好友+搜索)
├── report_output.h/.cpp          # 报告输出 (stdout + CSV)
├── scripts/
│   └── check_system.sh          # 向后兼容包装器
└── README.md                    # 本文件

test/scripts/
├── check_system.sh              # 统一环境检查 (unit/integration/stress)
└── run_tests.sh                 # 分层测试运行 (集成环境检查)
```

## 前置条件

### 1. 系统参数

```bash
# 一键检查 (根据模式自动调整检查严格度)
bash test/scripts/check_system.sh stress

# 手动设置
ulimit -n 200000
sudo sysctl -w net.ipv4.ip_local_port_range="1024 65535"
sudo sysctl -w net.ipv4.tcp_tw_reuse=1
sudo sysctl -w net.ipv4.tcp_max_syn_backlog=65535
sudo sysctl -w net.core.somaxconn=65535
```

### 2. 启动依赖服务

```bash
./bin/redis-server ./config/redis.conf
cd src/VerifyServer && npm start &
./cmake-build-debug/bin/StatusServer &
./cmake-build-debug/bin/ChatServer &
./cmake-build-debug/bin/GateServer &
```

## 构建

```bash
cd cmake-build-debug
cmake --build . --target IMTest
```

## 运行

```bash
export DYLD_LIBRARY_PATH=/usr/local/mysql-connector-c++-9.7.0/lib64:/usr/local/lib:/opt/homebrew/Cellar/openssl@3/3.6.2/lib:$DYLD_LIBRARY_PATH

# 分层运行 (自动调用环境检查)
bash test/run_tests.sh unit          # 仅 unit 测试
bash test/run_tests.sh stress        # 仅 stress 测试
bash test/run_tests.sh --check stress # 仅检查环境, 不执行

# 直接运行单个用例
./bin/IMTest --gtest_filter="BurstConnectTest.Connect_100_Fast"

# 跳过环境检查
SKIP_CHECK=1 ./bin/IMTest --gtest_filter="BurstConnectTest.Connect_1K"
```

### 按压力梯度运行

| 档位 | 命令 | 预计耗时 |
|------|------|----------|
| 验证 | `--gtest_filter="BurstConnectTest.Connect_100_Fast"` | ~10s |
| 1K 风暴 | `--gtest_filter="BurstConnectTest.Connect_1K"` | ~30s |
| 1K 持续 | `--gtest_filter="SustainedLoadTest.Sustain_1K_10min"` | 10min |
| 5K 风暴 | `--gtest_filter="BurstConnectTest.Connect_5K"` | ~60s |
| 5K 持续 | `--gtest_filter="SustainedLoadTest.Sustain_5K_10min"` | 10min |
| 10K 风暴 | `--gtest_filter="BurstConnectTest.Connect_10K"` | ~120s |
| 10K 持续 | `--gtest_filter="SustainedLoadTest.Sustain_10K_10min"` | 10min |
| 阶梯加压 | `--gtest_filter="RampUpTest.FindBreakingPoint"` | ~5min |
| 混合场景 | `--gtest_filter="MixedScenarioTest.Churn_5min"` | 5min |
| 1K 吞吐 | `--gtest_filter="ThroughputRampTest.Saturation_1K"` | ~5min |
| 5K 吞吐 | `--gtest_filter="ThroughputRampTest.Saturation_5K"` | ~5min |
| 1K 混合吞吐 | `--gtest_filter="MixedThroughputTest.Mixed_1K"` | ~8min |
| 5K 混合吞吐 | `--gtest_filter="MixedThroughputTest.Mixed_5K"` | ~8min |
| 10K 混合吞吐 | `--gtest_filter="MixedThroughputTest.Mixed_10K"` | ~10min |
| 全部 stress | `--gtest_filter="BurstConnectTest.*:RampUpTest.*:SustainedLoadTest.*:MixedScenarioTest.*:ThroughputRampTest.*:MixedThroughputTest.*"` | ~45min |

## 测试场景

### 1. BurstConnect — 瞬时连接风暴

| 用例 | 目标连接 | 批次大小 | 间隔 | 速率 | 超时 |
|------|----------|----------|------|------|------|
| Connect_100_Fast | 100 | 50 | 100ms | 500/s | 10s |
| Connect_1K | 1000 | 200 | 200ms | 1000/s | 30s |
| Connect_5K | 5000 | 500 | 200ms | 2500/s | 60s |
| Connect_10K | 10000 | 500 | 200ms | 2500/s | 120s |

### 2. RampUp — 阶梯加压 (找拐点)

| 参数 | 值 |
|------|-----|
| 起始/每档 | 500 / +500 |
| 上限 | 10000 (20 档) |
| 稳定时间 | 3s |
| 错误率阈值 | 10% |
| 在线率阈值 | 80% |

### 3. SustainedLoad — 持续负载

| 用例 | 连接数 | 持续时间 | 消息频率 |
|------|--------|----------|----------|
| Sustain_1K_10min | 1000 | 10min | 1% 用户每 5s |
| Sustain_5K_10min | 5000 | 10min | 1% 用户每 5s |
| Sustain_10K_10min | 10000 | 10min | 1% 用户每 5s |

### 4. MixedScenario — 混合真实场景

| 参数 | 值 |
|------|-----|
| 基础连接 | 500 |
| churn 速率 | 20 conn/s |
| 消息频率 | 10% 用户每 1s |
| 持续时间 | 5min |

### 5. ThroughputRamp — 消息吞吐探测 (纯聊天)

| 用例 | 连接数 | 速率档位 (msg/s/conn) | 稳定时间 | 终止条件 |
|------|--------|----------------------|----------|----------|
| Saturation_1K | 1000 | 1, 5, 10, 20, 30, 50, 70, 100 | 20s | P99>200ms 或 err>5% |
| Saturation_5K | 5000 | 1, 5, 10, 15, 20, 30, 50 | 20s | P99>200ms 或 err>5% |

### 6. MixedThroughput — 混合消息吞吐探测

| 用例 | 连接数 | 速率档位 (msg/s/conn) | 稳定时间 | 终止条件 |
|------|--------|----------------------|----------|----------|
| Mixed_1K | 1000 | 1, 3, 5, 10, 15, 20, 30, 50 | 20s | P99>200ms 或 err>5% |
| Mixed_5K | 5000 | 1, 3, 5, 10, 15, 20, 30 | 20s | P99>200ms 或 err>5% |
| Mixed_10K | 10000 | 1, 2, 3, 5, 10, 15, 20 | 20s | P99>200ms 或 err>5% |

消息混合比例: 70% 聊天 (`ID_CHAT_MSG_REQ`) + 20% 好友申请 (`ID_FRIEND_APPLY_REQ`) + 10% 用户搜索 (`ID_USER_SEARCH_REQ`)。

## 指标说明

| 指标 | 含义 |
|------|------|
| connect_attempts | 尝试连接次数 |
| connect_success | TCP 连接成功次数 |
| connect_failed | 连接失败次数 |
| connect_timeout | 连接超时次数 |
| handshake_success | 登录握手成功次数 |
| msg_sent / msg_recv | 消息发送/接收累计数 |
| chat_msg_sent / chat_msg_recv | 聊天消息发送/接收数 |
| friend_apply_sent / friend_apply_recv | 好友申请发送/接收数 |
| user_search_sent / user_search_recv | 用户搜索发送/接收数 |
| disconnect_ | 非预期断连次数 (从 ONLINE 断开时计数) |
| current_online | 当前在线连接数 |
| peak_online | 峰值在线连接数 |
| rtt_hist | 延迟直方图 (P50/P90/P95/P99/P999) |
| errorRate() | 断线率 = 断开 / (成功 + 断开) |

## 测试报告

测试结束后在 `cmake-build-debug/bin/` 目录生成 CSV 报告：

| 文件 | 场景 |
|------|------|
| `burst_connect_100_report.csv` | 100 连接风暴 |
| `burst_connect_1k_report.csv` | 1K 连接风暴 |
| `burst_connect_5k_report.csv` | 5K 连接风暴 |
| `burst_connect_10k_report.csv` | 10K 连接风暴 |
| `ramp_up_report.csv` | 阶梯加压 |
| `sustained_load_1k_report.csv` | 1K 持续负载 |
| `sustained_load_5k_report.csv` | 5K 持续负载 |
| `sustained_load_10k_report.csv` | 10K 持续负载 |
| `mixed_scenario_report.csv` | 混合场景 |
| `mixed_throughput_1k_report.csv` | 1K 混合吞吐 |
| `mixed_throughput_5k_report.csv` | 5K 混合吞吐 |
| `mixed_throughput_10k_report.csv` | 10K 混合吞吐 |
| `throughput_1k_report.csv` | 1K 聊天吞吐 |
| `throughput_5k_report.csv` | 5K 聊天吞吐 |

CSV 格式：
```
t,online,msg_sent,msg_recv,chat_sent,chat_recv,friend_sent,friend_recv,query_sent,query_recv,rtt_p50_us,rtt_p99_us,err_rate
10,1000,1010,1020,707,710,202,201,101,100,24307,54460,0.0000
...
```

## 加压测试流程

```
Connect_100_Fast (验证链路)
       ↓
Connect_1K + Sustain_1K_10min (基准)
       ↓
Connect_5K + Sustain_5K_10min (中等压力)
       ↓
Connect_10K + Sustain_10K_10min (极限)
       ↓
RampUpTest (找实际拐点)
       ↓
ThroughputRampTest (纯聊天吞吐饱和)
       ↓
MixedThroughputTest (混合消息吞吐饱和)
```

每档通过标准：
- 在线率 ≥ 85% (极限档) / ≥ 95% (常规档)
- RTT P99 < 100ms
- 错误率 < 5%

## 已知问题

| 问题 | 状态 | 说明 |
|------|------|------|
| RTT 测量 | 🔧 待修复 | `recordRtt` 使用 `msgId-1` 查找发送时间，心跳与登录消息 ID 需对齐 |
| RampUp 无数据 | 🔧 待修复 | ReportOutput 集成问题，CSV 输出为空 |
| Mixed 错误率虚高 | ✅ 已修复 | `disconnect_` 重复计数 → 改用 `exchange()` 原子操作 |
| check_system 提前退出 | ✅ 已修复 | `set -e` + `((PASS++))` 冲突 → 改用 `set -u` + 增量函数 |
