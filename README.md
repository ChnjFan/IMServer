# IMServer - 即时通讯服务器

一个高性能的即时通讯服务器，包含 GateServer 和 ChatServer 两个组件。

## 组件

- GateServer：网关服务器，负责客户端连接管理和协议路由。
- ChatServer：核心聊天服务器，处理消息处理、房间管理和用户交互。

## 依赖

### 必需库

| 库 | 版本 | 描述 |
|---------|---------|-------------|
| Boost | >= 1.90 | C++ 网络和线程库 |
| jsoncpp | >= 1.9.6 | JSON 解析和序列化 |
| gRPC | >= 1.77 | 高性能 RPC 框架 |
| Protobuf | >= 3.31 | 数据序列化协议 |

### 使用的 Boost 组件

- **boost_system** - 系统级操作和错误处理
- **boost_thread** - 多线程支持
- **boost_filesystem** - 文件系统操作

### 安装方式

#### macOS (Homebrew)
```bash
brew install boost jsoncpp grpc protobuf
```

#### Ubuntu/Debian
```bash
sudo apt-get install libboost-all-dev libjsoncpp-dev libgrpc-dev libprotobuf-dev protobuf-compiler-grpc
```

#### CentOS/RHEL
```bash
sudo yum install boost-devel jsoncpp-devel grpc-devel protobuf-devel
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

| 选项 | 默认值 | 描述 |
|--------|---------|-------------|
| `BUILD_GATE_SERVER` | ON | 构建 GateServer 可执行文件 |
| `BUILD_CHAT_SERVER` | ON | 构建 ChatServer 可执行文件 |
| `BUILD_SHARED_LIBS` | ON | 构建共享库 |
| `CMAKE_BUILD_TYPE` | Debug | 构建类型 (Debug/Release/RelWithDebInfo) |

## 运行

### GateServer
```bash
./bin/GateServer
```

### ChatServer
```bash
./bin/ChatServer
```
