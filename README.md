# IMServer - C++ Instant Messaging Server

## 项目概述
IMServer是一个基于C++开发的高性能即时通讯服务端，支持多种通讯协议，提供稳定可靠的消息传输和用户管理功能。

## 功能特性

- **多协议支持**：WebSocket、TCP等协议
- **高性能**：基于事件驱动的网络模型
- **可扩展**：模块化设计，易于扩展新功能
- **安全**：支持SSL/TLS加密，身份认证和授权
- **实时消息**：单聊、群聊、广播消息
- **持久化**：消息历史记录和用户数据存储
- **负载均衡**：支持水平扩展

## 技术栈

- **开发语言**：C++17
- **网络库**：可使用boost.asio或muduo
- **数据库**：MySQL (用户数据)、Redis (缓存和会话)
- **构建工具**：CMake
- **日志库**：spdlog
- **JSON处理**：nlohmann/json

## 目录结构

```
IMServer/
├── src/                  # 源代码目录
│   ├── core/            # 核心功能模块
│   │   ├── Server.h     # 服务器核心类
│   │   ├── Client.h     # 客户端连接管理
│   │   └── Session.h    # 会话管理
│   ├── network/         # 网络模块
│   │   ├── TcpServer.h  # TCP服务器实现
│   │   ├── WebSocketServer.h # WebSocket服务器实现
│   │   └── EventLoop.h  # 事件循环
│   ├── protocol/        # 协议模块
│   │   ├── Protocol.h   # 协议基类
│   │   ├── Message.h    # 消息结构
│   │   └── Parser.h     # 协议解析器
│   ├── storage/         # 存储模块
│   │   ├── Database.h   # 数据库接口
│   │   ├── Redis.h      # Redis客户端
│   │   └── UserDao.h    # 用户数据访问对象
│   ├── utils/           # 工具模块
│   │   ├── Logger.h     # 日志工具
│   │   ├── Config.h     # 配置管理
│   │   └── Utils.h      # 通用工具函数
│   └── main/            # 主程序入口
│       └── main.cpp     # 程序主函数
├── include/             # 头文件目录（与src结构对应）
├── config/              # 配置文件目录
│   └── server.conf      # 服务器配置文件
├── test/                # 测试目录
├── docs/                # 文档目录
├── third_party/         # 第三方库目录
├── CMakeLists.txt       # CMake构建文件
├── README.md            # 项目说明文档
├── LICENSE              # 许可证
└── .gitignore           # Git忽略文件
```

## 编译和运行

### 依赖安装

- **CMake** (>= 3.10)
- **C++17兼容编译器** (GCC 7+, Clang 5+, MSVC 2017+)
- **MySQL客户端库**
- **Redis客户端库** (如hiredis)
- **Boost库** (可选，如使用boost.asio)

### 编译步骤

```bash
# 克隆仓库
# git clone <repository-url>
cd IMServer

# 创建构建目录
mkdir build
cd build

# 配置项目
cmake ..

# 编译
make -j4  # 或使用 Visual Studio 在Windows上编译

# 安装（可选）
sudo make install
```

### 运行服务器

```bash
# 直接运行
./imserver

# 指定配置文件运行
./imserver -c /path/to/server.conf
```

## 配置说明

配置文件位于 `config/server.conf`，主要配置项包括：

- **服务器设置**：端口、主机地址、线程池大小、最大连接数
- **日志设置**：日志级别、文件路径、最大大小
- **数据库设置**：MySQL连接信息
- **Redis设置**：Redis连接信息
- **协议设置**：协议类型、心跳间隔、最大消息大小
- **安全设置**：SSL证书路径、token过期时间

详细配置说明请参考 `docs/configuration.md`。

## 开发指南

### 模块开发

1. **核心模块**：处理服务器的启动、停止、客户端连接管理
2. **网络模块**：实现不同协议的网络通信
3. **协议模块**：定义消息格式和解析规则
4. **存储模块**：处理数据持久化和缓存
5. **工具模块**：提供日志、配置等通用功能

### 代码规范

- 使用C++17标准
- 遵循Google C++代码规范
- 使用智能指针管理内存
- 避免使用全局变量
- 添加适当的注释

## 测试

```bash
# 运行单元测试
cd build
make test

# 运行集成测试
./test/integration_test
```

## 部署

### 生产环境部署

1. 编译为Release版本
2. 配置生产环境的server.conf
3. 设置适当的日志级别
4. 配置SSL证书
5. 启动服务器并设置为守护进程

### 监控和维护

- 查看日志文件 `logs/imserver.log`
- 使用Redis监控工具查看缓存状态
- 定期备份数据库

## 贡献

欢迎提交Issue和Pull Request！

## 许可证

MIT License

## 联系方式

如有问题或建议，请联系项目维护者。


