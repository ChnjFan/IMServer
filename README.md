# IMServer - 即时通讯服务器

一个高性能的即时通讯服务器，包含 GateServer 和 ChatServer 两个组件。

## 组件

- GateServer：网关服务器，负责客户端连接管理和协议路由。
- ChatServer：核心聊天服务器，处理消息处理、房间管理和用户交互。
- StatusServer：状态服务器，管理用户在线状态和好友关系。

## 依赖

### 必需库

| 库        | 版本       | 描述          |
| -------- | -------- | ----------- |
| Boost    | >= 1.90  | C++ 网络和线程库  |
| jsoncpp  | >= 1.9.6 | JSON 解析和序列化 |
| gRPC     | >= 1.77  | 高性能 RPC 框架  |
| Protobuf | >= 3.31  | 数据序列化协议     |
| hiredis  | >= 1.3   | Redis C 客户端库   |

### 使用的 Boost 组件

- **boost\_system** - 系统级操作和错误处理
- **boost\_thread** - 多线程支持
- **boost\_filesystem** - 文件系统操作

### 安装方式

#### macOS (Homebrew)

```bash
brew install boost jsoncpp grpc protobuf hiredis
```

#### Ubuntu/Debian

```bash
sudo apt-get install libboost-all-dev libjsoncpp-dev libgrpc-dev libprotobuf-dev protobuf-compiler-grpc libhiredis-dev
```

#### CentOS/RHEL

```bash
sudo yum install boost-devel jsoncpp-devel grpc-devel protobuf-devel hiredis-devel
```

#### 源码安装 (hiredis)

如果系统包管理器中的 hiredis 版本过旧，可以从源码安装：

```bash
git clone https://github.com/redis/hiredis.git
cd hiredis
make
sudo make install
```

### Node.js 依赖 (VerifyServer)

```bash
cd src/VerifyServer
npm install
```

## 构建

### 前置条件

- CMake >= 3.15
- 支持 C++17 的编译器 (GCC >= 8, Clang >= 9, MSVC >= 2019)

### 构建步骤

```bash
# 创建构建目录
mkdir build && cd build

# 配置 CMake
cmake ..

# 可选：只构建特定组件
cmake -DBUILD_GATE_SERVER=OFF ..  # 禁用 GateServer
cmake -DBUILD_CHAT_SERVER=OFF .. # 禁用 ChatServer

# 构建
make -j$(nproc)
```

### 构建选项

| 选项                  | 默认值   | 描述                                  |
| ------------------- | ----- | ----------------------------------- |
| `BUILD_GATE_SERVER` | ON    | 构建 GateServer 可执行文件                 |
| `BUILD_CHAT_SERVER` | ON    | 构建 ChatServer 可执行文件                 |
| `BUILD_SHARED_LIBS` | ON    | 构建共享库                               |
| `CMAKE_BUILD_TYPE`  | Debug | 构建类型 (Debug/Release/RelWithDebInfo) |

## 运行

### GateServer

```bash
./bin/GateServer
```

### ChatServer

```bash
./bin/ChatServer
```

### StatusServer

```bash
./bin/StatusServer
```

## 测试框架

### 构建测试

```bash
cmake --build cmake-build-debug --target IMTest
```

### 运行测试

```bash
# 单元测试（快速，无需外部依赖）
./cmake-build-debug/bin/IMTest --gtest_filter="ProtocolTest.*"

# 或通过封装脚本运行（自动设置 libssl 运行时路径）
./test/run_tests.sh unit        # 仅单元测试
./test/run_tests.sh integration # 集成测试（需 Redis + MySQL + 各服务器在线）
./test/run_tests.sh all         # 全部
```

需要在线服务的测试在 Redis 不可用时自动跳过，不会报错。

### 生成测试报告

稳定性 / 性能测试运行后生成 CSV 时序文件，用可视化脚本输出图表：

```bash
# 安装依赖
pip3 install matplotlib --break-system-packages

# 生成报告图（默认读 stability_report.csv，输出 PNG）
python3 scripts/plot_report.py

# 指定路径
python3 scripts/plot_report.py cmake-build-debug/bin/stability_report.csv report.png
```

生成的 PNG 包含 4 个子图：

1. **延迟时序**（P50/P99/Avg）— 判断系统是否稳定
2. **吞吐量 QPS 时序** — 判断吞吐是否平稳
3. **错误率时序** — 定位故障时间点
4. **累计请求 & 累计错误**（双 Y 轴）— 整体趋势

同时在终端输出摘要：测试时长、总请求数、错误率、延迟末值、平均 QPS。

#### CSV 字段说明

| 字段 | 含义 |
| ---- | ---- |
| `timestamp` | 采样时间戳（Unix 秒） |
| `count` | 累计请求数 |
| `p50_us` | P50 延迟（微秒），中位数 |
| `p99_us` | P99 延迟（微秒），尾部延迟 |
| `avg_us` | 平均延迟（微秒） |
| `errors` | 累计错误数 |
| `error_rate` | 错误率（errors / count） |

### 测试目录结构

```
test/
├── run_tests.sh               # 分层运行脚本
├── framework/                 # 共享测试工具
│   ├── protocol.h/cpp         # TCP 二进制协议编解码
│   ├── chat_test_client.h/cpp # ChatServer TCP 客户端
│   ├── http_test_client.h/cpp # GateServer HTTP 客户端
│   ├── grpc_test_client.h     # gRPC 通用客户端
│   ├── account_manager.h/cpp  # 测试账号管理（自动清理）
│   ├── fixture_base.h         # 集成测试基类（Redis 探测 + 跳过）
│   ├── metrics.h/cpp          # 延迟/吞吐/错误率统计
│   ├── resource_monitor.h     # fd / RSS 泄漏检测
│   ├── report.h/cpp           # 报告输出（摘要 + CSV）
│   └── stability_runner.h/cpp # 长跑场景编排
├── gate/                      # GateServer 测试
├── status/                    # StatusServer 测试
├── chat/                      # ChatServer 测试
├── integration/               # 跨服务 / 稳定性测试
└── perf/                      # 性能压测
```

