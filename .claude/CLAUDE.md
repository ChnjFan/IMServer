# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

```bash
# The project uses CMake with Ninja generator. Pre-configured build directory.
# NOTE: `ninja` may not be on PATH; use `cmake --build` which locates the generator:
cmake --build cmake-build-debug --target <TargetName>

# Build all servers + tests at once:
cmake --build cmake-build-debug

# Build outputs go to `cmake-build-debug/bin/`.
# The build also copies `config.ini` to the output directory.

# If the ninja binary IS on your PATH, you can also use:
#   cd cmake-build-debug && ninja <TargetName>
```

### Build targets

| Target | Type | Notes |
|--------|------|-------|
| `GateServer` | server | HTTP gateway |
| `ChatServer` | server | core IM |
| `StatusServer` | server | gRPC status/routing |
| `ResourceServer` | server | file resources |
| `IMTest` | test executable | all gtest cases |

The `cmake --build` command works even when `ninja` is not on the shell PATH because CMake knows the generator location from the configured build tree.

The cmake-build-debug directory is already configured with Ninja and uses C++17 (GNU/clang extensions enabled).

## Testing

```bash
# Build the test executable:
cmake --build cmake-build-debug --target IMTest

# Run unit tests only (fast, no external deps):
./test/run_tests.sh unit

# Run integration tests (requires Redis + MySQL + VerifyServer + GateServer + StatusServer):
./test/run_tests.sh integration

# Run a specific gtest filter directly:
./cmake-build-debug/bin/IMTest --gtest_filter="ProtocolTest.*"
```

Test sources live under `test/` (mirrors the spec at `docs/superpowers/specs/2026-07-11-cpp-testing-framework-design.md`):

- `test/framework/` — shared utilities: protocol codec, HTTP/gRPC clients, fixtures, metrics
- `test/gate/`, `test/status/`, `test/chat/` — per-server integration tests
- `test/integration/` — cross-service tests
- `test/perf/` — stability + performance scenarios

Integration tests gracefully skip when Redis is unavailable. For the crash at shutdown (`condition_variable timed_wait failed`), see the macOS runtime note below — it is a known gtest/libc++ + detached-thread interaction; the test results are valid before the crash.

## Run

```bash
# Start Redis first (required by all servers)
./bin/redis-server ./config/redis.conf

# Start VerifyServer (Node.js — email verification codes)
cd src/VerifyServer && npm start

# Start StatusServer (gRPC, port 50052)
./cmake-build-debug/bin/StatusServer

# Start ChatServer (TCP for clients on config.ini port, gRPC on RPC port)
./cmake-build-debug/bin/ChatServer

# Start GateServer (HTTP gateway on config.ini port)
./cmake-build-debug/bin/GateServer
```

All servers read from `config.ini` in their working directory. Redis must be running before any C++ server starts.

### macOS runtime note (libssl)

The binaries link `libssl.3.dylib` and `libcrypto.3.dylib` by bare name (shipped with mysql-connector-c++). If they fail to load at runtime with `dyld: Library not loaded: libssl.3.dylib`, run with the library path set:

```bash
export DYLD_LIBRARY_PATH="/usr/local/mysql-connector-c++-9.7.0/lib64:/opt/homebrew/lib:${DYLD_LIBRARY_PATH:-}"
./cmake-build-debug/bin/ChatServer
```

The `test/run_tests.sh` script sets this internally, so tests work without extra env.

## Architecture

IMServer is a multi-service instant messaging backend. Four independent server processes communicate via gRPC:

### Server Components

| Server | Language | Protocol | Role |
|--------|----------|----------|------|
| **GateServer** | C++ | HTTP (Boost.Beast) | Client-facing REST API gateway. Handles registration, login, password reset. Routes to VerifyServer/StatusServer via gRPC. |
| **ChatServer** | C++ | Custom TCP + gRPC | Core IM logic. Persistent TCP connections to clients using a custom binary protocol. Message routing, friend management, user info. Supports horizontal scaling (multiple instances). |
| **StatusServer** | C++ | gRPC | User online status and login routing. Acts as a load balancer — assigns incoming users to the least-loaded ChatServer instance. |
| **VerifyServer** | Node.js | gRPC | Generates email verification codes, stores them in Redis with TTL, sends via nodemailer. |

### Request Flow

1. **Registration/Login**: Client → HTTP → GateServer → VerifyServer (gRPC, for verify code) or StatusServer (gRPC, to get assigned ChatServer host/token)
2. **Chat**: Client → TCP → ChatServer. GateServer hands the client a token + ChatServer address after login; client then opens a persistent TCP connection to ChatServer.

### Inter-Server Communication

- gRPC proto definitions live in `proto/message.proto` (compiled to `src/proto/message.pb.h` and `message.grpc.pb.h`)
- Each C++ server that calls gRPC uses `ServiceConnPool<ServiceType>` (in `src/proto/ServiceConnPool.h`) — a blocking connection pool
- ChatServer instances communicate peer-to-peer via gRPC (`ChatService`) for cross-server message delivery and friend notifications

### ChatServer Internal Architecture (where most complexity lives)

```
TCP Socket (Session) → MsgNode queue → ChatLogicSystem worker thread
                                            ↓
                                   ThreadPool (CPU-bound work)
                                            ↓
                              MySQL (UserMgr/FriendCache) + Redis (UserInfoCache)
```

- **`Session`** — one per connected client. Reads custom binary protocol: 4-byte header (2B msgId + 2B size), then body. Has a send queue for async writes.
- **`ChatLogicSystem`** — singleton with a dedicated worker thread pulling `LogicNode` messages from a queue. Registers handlers by `MessageID` enum. Dispatches CPU-bound work to a `ThreadPool`.
- **`UserMgr`** — maps uid → Session for online users.
- **`ChatServer`** — manages all sessions, runs a periodic heartbeat check timer.

### GateServer Internal Architecture

```
HTTP Request → HttpConnection → LogicSystem (path-based handler dispatch)
```

- **`HttpConnection`** — wraps a Beast HTTP connection with URL parsing, timeout handling.
- **`LogicSystem`** — singleton that registers GET/POST handlers by URL path. All business logic is in lambda handlers inside `LogicSystem.cpp`.

### Key Shared Libraries (`src/base/`)

- **`ConfigMgr`** — INI-style config file parser (`config.ini`). Access via `ConfigMgr::getInstance()["Section"]["Key"]`.
- **`RedisMgr`** — hiredis wrapper with connection pooling. Used for verification codes, online status, friend applications, distributed locks.
- **`MysqlPool`** / **`MysqlMgr`** — MySQL connection pool (mysql-connector-cpp). GateServer has its own `MysqlMgr`; ChatServer has a more complex DAO layer under `db/mysql/`.
- **`AsioIOServicePool`** — round-robin pool of `io_context` instances (N = hardware concurrency - 1).
- **`Singleton<T>`** — CRTP singleton base using `std::call_once` + `shared_ptr`.
- **`DistLock`** — Redis-based distributed lock (SETNX with timeout). `DistLockGuard` RAII wrapper.

### Custom TCP Binary Protocol

Used between client and ChatServer. Each message:
- 2 bytes: Message ID (see `enum class MessageID` in `src/base/const.h`)
- 2 bytes: body size
- N bytes: JSON body

### Database Schema

See `scripts/*.sql` for the full MySQL schema. Key tables: `user_base_info`, `user_profile`, `friend_info`, `friend_apply`.

## Configuration

`config.ini` uses INI sections. Key sections:
- `[GateServer]` — HTTP listen port
- `[VerifyServer]` / `[StatusServer]` — gRPC host:port for GateServer to call
- `[ChatServer]` / `[ChatServer2]` — TCP port + gRPC port per instance. `[PeerChatServers]` lists other instances for cross-server routing.
- `[Redis]` / `[Mysql]` — database connections

## Notes

- The project is in active development on `beta/v0.0.1`. Code marked "todo 待修复" in `ChatLogicSystem` is known work-in-progress (conversation history, file upload, chat messaging).
- ChatServer uses Redis for distributed login counting and server selection (least-connections load balancing in StatusServer).
- Boost.Beast and Boost.Asio are the core networking libraries — familiarity with their async patterns (sockets, timers, strands) is important for any networking changes.
