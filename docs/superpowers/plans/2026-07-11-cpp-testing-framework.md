# C++ Testing Framework Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a C++ testing framework for IMServer covering correctness, stability, and performance across GateServer, StatusServer, and ChatServer.

**Architecture:** Three-phase rollout — Phase 1 establishes gtest scaffold + protocol codec + Gate/Status integration tests; Phase 2 adds ChatTestClient + stability runner + metrics; Phase 3 adds performance suite + profiler integration. All phases reuse the `test_framework` static library.

**Tech Stack:** C++17, Google Test (gtest), CMake, FetchContent, Boost.Beast (HTTP client), gRPC, jsoncpp

## Global Constraints

- C++17 standard, GNU/clang extensions (match existing `CMAKE_CXX_EXTENSIONS OFF`)
- Project uses jsoncpp (not nlohmann/json) — all JSON code must use `Json::Value`
- Protocol frame: 2B msgId (little-endian uint16) + 2B bodyLen (little-endian uint16) + N B JSON body
- `BUILD_TEST` CMake option gates test compilation (already exists in main CMakeLists.txt)
- All test executables output to `cmake-build-debug/bin/`
- Test label convention: `[unit]`, `[integration]`, `[stability]`, `[perf]`
- Account naming: `test_{tag}_{seq}@test.com` for isolation
- gtest introduced via FetchContent (no system install required)

---

## Phase 1: 基线正确性（Week 1-2）

### Task 1: CMake + gtest 脚手架

**Files:**
- Modify: `test/CMakeLists.txt`
- Modify: `CMakeLists.txt` (main, line near `BUILD_TEST` section)

**Interfaces:**
- Produces: `IMTest` executable at `cmake-build-debug/bin/IMTest`
- Produces: `test_framework` static library (Phase 1+ reusable)

- [ ] **Step 1: Replace test/CMakeLists.txt with gtest scaffold**

Replace the entire contents of `test/CMakeLists.txt`:

```cmake
include(FetchContent)

# gtest via FetchContent — no system install needed
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
# For Windows: prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# test_framework — shared test utilities (built once, linked by all tests)
add_library(test_framework STATIC
    framework/protocol.cpp
    framework/account_manager.cpp
    framework/http_test_client.cpp
)
target_include_directories(test_framework
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/framework
)
target_link_libraries(test_framework
    PUBLIC base
    PUBLIC jsoncpp_deps
    PUBLIC Boost::filesystem
)

# IMTest — single executable hosting all gtest cases
add_executable(IMTest
    gate/gate_integration_test.cpp
    status/status_integration_test.cpp
)
target_link_libraries(IMTest
    PRIVATE test_framework
    PRIVATE GTest::gtest_main
    PRIVATE GTest::gmock
)

include(GoogleTest)
gtest_discover_tests(IMTest)
```

- [ ] **Step 2: Create framework directory structure**

```bash
mkdir -p test/framework test/gate test/status test/chat test/integration test/perf
```

- [ ] **Step 3: Verify build compiles**

```bash
cd cmake-build-debug && ninja IMTest
```
Expected: Build succeeds (gtest downloaded via FetchContent, test_framework compiled, IMTest links)

- [ ] **Step 4: Commit**

```bash
git add test/CMakeLists.txt
git commit -m "test: add gtest scaffold via FetchContent with test_framework library"
```

---

### Task 2: Protocol 编解码 + 单元测试

**Files:**
- Create: `test/framework/protocol.h`
- Create: `test/framework/protocol.cpp`
- Create: `test/framework/protocol_test.cpp`

**Interfaces:**
- Produces: `encode(uint16_t msgId, const Json::Value& body) -> std::string`
- Produces: `decode(const char* buf, size_t len) -> std::vector<DecodedFrame>`
- Produces: `DecodedFrame` struct with `msgId`, `body` fields

- [ ] **Step 1: Write protocol.h**

Create `test/framework/protocol.h`:

```cpp
#ifndef IMSERVER_PROTOCOL_H
#define IMSERVER_PROTOCOL_H

#include <cstdint>
#include <string>
#include <vector>
#include <json/json.h>

#pragma pack(push, 1)
struct FrameHeader {
    uint16_t msgId;
    uint16_t bodyLen;
};
#pragma pack(pop)

struct DecodedFrame {
    uint16_t msgId;
    Json::Value body;
};

// Encode msgId + JSON body into a binary frame (suitable for direct send)
std::string encode(uint16_t msgId, const Json::Value& body);

// Decode binary buffer into complete frames, handling split/sticky packets.
// Incomplete data is kept internally for the next call.
std::vector<DecodedFrame> decode(const char* buf, size_t len);

#endif // IMSERVER_PROTOCOL_H
```

- [ ] **Step 2: Write protocol.cpp**

Create `test/framework/protocol.cpp`:

```cpp
#include "protocol.h"
#include <cstring>
#include <boost/endian/conversion.hpp>

// Internal buffer for split-packet reassembly
static std::string s_recvBuffer;

std::string encode(uint16_t msgId, const Json::Value& body) {
    StreamWriterBuilder writer;
    std::string bodyStr = writeString(writer, body, nullptr);

    FrameHeader header;
    header.msgId = boost::endian::native_to_little(msgId);
    header.bodyLen = boost::endian::native_to_little(static_cast<uint16_t>(bodyStr.size()));

    std::string frame;
    frame.reserve(sizeof(FrameHeader) + bodyStr.size());
    frame.append(reinterpret_cast<const char*>(&header), sizeof(FrameHeader));
    frame.append(bodyStr);
    return frame;
}

std::vector<DecodedFrame> decode(const char* buf, size_t len) {
    s_recvBuffer.append(buf, len);
    std::vector<DecodedFrame> frames;

    while (s_recvBuffer.size() >= sizeof(FrameHeader)) {
        FrameHeader header;
        std::memcpy(&header, s_recvBuffer.data(), sizeof(FrameHeader));
        header.msgId = boost::endian::little_to_native(header.msgId);
        header.bodyLen = boost::endian::little_to_native(header.bodyLen);

        if (s_recvBuffer.size() < sizeof(FrameHeader) + header.bodyLen) {
            break; // incomplete — wait for more data
        }

        DecodedFrame frame;
        frame.msgId = header.msgId;
        std::string bodyStr = s_recvBuffer.substr(sizeof(FrameHeader), header.bodyLen);

        CharReaderBuilder reader;
        std::string errs;
        Json::CharReader* charReader = reader.newCharReader();
        charReader->parse(bodyStr.data(), bodyStr.data() + bodyStr.size(),
                         &frame.body, &errs);
        delete charReader;

        frames.push_back(std::move(frame));
        s_recvBuffer.erase(0, sizeof(FrameHeader) + header.bodyLen);
    }
    return frames;
}

// Reset internal buffer — for tests only
void protocol_reset_buffer() {
    s_recvBuffer.clear();
}
```

- [ ] **Step 3: Add `protocol_reset_buffer` declaration to protocol.h**

Add this declaration to `protocol.h` (after `decode`):

```cpp
// Test helper: clear the internal reassembly buffer
void protocol_reset_buffer();
```

- [ ] **Step 4: Write protocol_test.cpp**

Create `test/framework/protocol_test.cpp`:

```cpp
#include <gtest/gtest.h>
#include "protocol.h"

TEST(ProtocolTest, EncodeDecodeRoundTrip) {
    protocol_reset_buffer();
    Json::Value body;
    body["uid"] = 123;
    body["name"] = "alice";

    std::string frame = encode(3001, body);
    EXPECT_EQ(frame.size(), sizeof(FrameHeader) + Json::StreamWriterBuilder().newWriter()->write(body).size());

    auto frames = decode(frame.data(), frame.size());
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].msgId, 3001);
    EXPECT_EQ(frames[0].body["uid"].asInt(), 123);
    EXPECT_EQ(frames[0].body["name"].asString(), "alice");
}

TEST(ProtocolTest, SplitPacket) {
    protocol_reset_buffer();
    Json::Value body;
    body["msg"] = "hello world, this is a longer body for split testing";

    std::string frame = encode(1004, body);

    // Feed half
    auto frames = decode(frame.data(), frame.size() / 2);
    EXPECT_EQ(frames.size(), 0u); // incomplete — no frames yet

    // Feed rest
    frames = decode(frame.data() + frame.size() / 2, frame.size() - frame.size() / 2);
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].body["msg"].asString(), "hello world, this is a longer body for split testing");
}

TEST(ProtocolTest, StickyPackets) {
    protocol_reset_buffer();
    Json::Value b1; b1["a"] = 1;
    Json::Value b2; b2["b"] = 2;

    std::string combined = encode(1005, b1) + encode(1006, b2);

    auto frames = decode(combined.data(), combined.size());
    ASSERT_EQ(frames.size(), 2u);
    EXPECT_EQ(frames[0].msgId, 1005);
    EXPECT_EQ(frames[1].msgId, 1006);
    EXPECT_EQ(frames[1].body["b"].asInt(), 2);
}
```

- [ ] **Step 5: Update test/CMakeLists.txt to include protocol_test.cpp**

Add `framework/protocol_test.cpp` to the `IMTest` executable sources.

- [ ] **Step 6: Build and run**

```bash
cd cmake-build-debug && ninja IMTest
./bin/IMTest --gtest_filter="ProtocolTest.*"
```
Expected: 3 tests pass

- [ ] **Step 7: Commit**

```bash
git add test/framework/
git commit -m "test: add TCP binary protocol encode/decode with unit tests"
```

---

### Task 3: HTTP Test Client (GateServer 用)

**Files:**
- Create: `test/framework/http_test_client.h`
- Create: `test/framework/http_test_client.cpp`

**Interfaces:**
- Produces: `HttpTestClient` class with `post(path, json) -> HttpResponse` and `get(path) -> HttpResponse`
- Produces: `HttpResponse` struct: `{ int code; Json::Value body; }`

- [ ] **Step 1: Write http_test_client.h**

```cpp
#ifndef IMSERVER_HTTP_TEST_CLIENT_H
#define IMSERVER_HTTP_TEST_CLIENT_H

#include <string>
#include <functional>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <json/json.h>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

struct HttpResponse {
    int code;
    Json::Value body;
};

class HttpTestClient {
public:
    HttpTestClient(const std::string& host, uint16_t port);
    ~HttpTestClient();

    HttpResponse post(const std::string& path, const Json::Value& body);
    HttpResponse get(const std::string& path);

private:
    std::string host_;
    std::string port_;
    net::io_context ioc_;
};

#endif
```

- [ ] **Step 2: Write http_test_client.cpp**

```cpp
#include "http_test_client.h"
#include <boost/beast/http.hpp>
#include <json/json.h>

HttpTestClient::HttpTestClient(const std::string& host, uint16_t port)
    : host_(host), port_(std::to_string(port)) {}

HttpTestClient::~HttpTestClient() = default;

HttpResponse HttpTestClient::post(const std::string& path, const Json::Value& body) {
    tcp::resolver resolver(ioc_);
    beast::tcp_stream stream(ioc_);

    auto const results = resolver.resolve(host_, port_);
    stream.connect(results);

    Json::StreamWriterBuilder writer;
    std::string bodyStr = Json::writeString(writer, body);

    http::request<http::string_body> req{http::verb::post, path, 11};
    req.set(http::field::host, host_);
    req.set(http::field::content_type, "application/json");
    req.set(http::field::content_length, std::to_string(bodyStr.size()));
    req.body() = bodyStr;
    req.prepare_payload();

    http::write(stream, req);

    beast::flat_buffer buffer;
    http::response<http::string_body> res;
    http::read(stream, buffer, res);

    HttpResponse rsp;
    rsp.code = static_cast<int>(res.result_int());

    Json::CharReaderBuilder reader;
    std::string errs;
    Json::CharReader* charReader = reader.newCharReader();
    charReader->parse(res.body().data(),
                      res.body().data() + res.body().size(),
                      &rsp.body, &errs);
    delete charReader;

    beast::error_code ec;
    stream.socket().shutdown(tcp::socket::shutdown_both, ec);
    return rsp;
}

HttpResponse HttpTestClient::get(const std::string& path) {
    tcp::resolver resolver(ioc_);
    beast::tcp_stream stream(ioc_);

    auto const results = resolver.resolve(host_, port_);
    stream.connect(results);

    http::request<http::string_body> req{http::verb::get, path, 11};
    req.set(http::field::host, host_);
    http::write(stream, req);

    beast::flat_buffer buffer;
    http::response<http::string_body> res;
    http::read(stream, buffer, res);

    HttpResponse rsp;
    rsp.code = static_cast<int>(res.result_int());
    return rsp;
}
```

- [ ] **Step 3: Update test/CMakeLists.txt**

`http_test_client.cpp` is already in `test_framework` sources (Task 1).

- [ ] **Step 4: Build and verify no linker errors**

```bash
cd cmake-build-debug && ninja IMTest
```
Expected: links successfully

- [ ] **Step 5: Commit**

```bash
git add test/framework/http_test_client.h test/framework/http_test_client.cpp
git commit -m "test: add HTTP test client for GateServer integration tests"
```

---

### Task 4: gRPC Test Client (StatusServer 用)

**Files:**
- Create: `test/framework/grpc_test_client.h`

**Interfaces:**
- Produces: `GrpcTestClient` template wrapping `ServiceConnPool` for test use

- [ ] **Step 1: Write grpc_test_client.h**

```cpp
#ifndef IMSERVER_GRPC_TEST_CLIENT_H
#define IMSERVER_GRPC_TEST_CLIENT_H

#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>

template<typename ServiceType>
class GrpcTestClient {
public:
    GrpcTestClient(const std::string& host, uint16_t port)
        : channel_(grpc::CreateChannel(host + ":" + port,
                   grpc::InsecureChannelCredentials())),
          stub_(ServiceType::NewStub(channel_)) {}

    std::unique_ptr<typename ServiceType::Stub>& stub() { return stub_; }

private:
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<typename ServiceType::Stub> stub_;
};

#endif
```

- [ ] **Step 2: Update test/CMakeLists.txt**

Add `grpc_deps` to `test_framework` link libraries.

- [ ] **Step 3: Build and verify**

```bash
cd cmake-build-debug && ninja IMTest
```
Expected: links successfully

- [ ] **Step 4: Commit**

```bash
git add test/framework/grpc_test_client.h
git commit -m "test: add gRPC test client wrapper"
```

---

### Task 5: Fixture 基类 + AccountManager

**Files:**
- Create: `test/framework/fixture_base.h`
- Create: `test/framework/account_manager.h`
- Create: `test/framework/account_manager.cpp`

**Interfaces:**
- Produces: `IntegrationTestBase` gtest fixture (skips if Redis/MySQL unreachable)
- Produces: `AccountManager` with `acquire(tag) -> TestAccount`, `releaseAll()`, `acquireBatch(n, tag)`

- [ ] **Step 1: Write account_manager.h**

```cpp
#ifndef IMSERVER_ACCOUNT_MANAGER_H
#define IMSERVER_ACCOUNT_MANAGER_H

#include <set>
#include <string>
#include <vector>
#include "json/json.h"

struct TestAccount {
    int uid;
    std::string email;
    std::string token;
};

class AccountManager {
public:
    AccountManager();
    ~AccountManager();

    // Acquire a unique test account (registers via HTTP to GateServer)
    TestAccount acquire(const std::string& tag);

    // Batch acquire for multi-user scenarios
    std::vector<TestAccount> acquireBatch(int n, const std::string& tag);

    // Release a specific account
    void release(int uid);

    // Release all acquired accounts
    void releaseAll();

private:
    std::set<int> heldUids_;
    int userCount = 0;
    std::string gateHost_;
    uint16_t gatePort_;

    // HTTP helper: GET verify code from Redis
    std::string fetchVerifyCode(const std::string& email);

    // HTTP helper: register via GateServer
    TestAccount registerAccount(const std::string& email);
};

#endif
```

- [ ] **Step 2: Write account_manager.cpp**

```cpp
#include "account_manager.h"
#include "http_test_client.h"

#include <chrono>
#include <random>
#include <fstream>
#include <iostream>

externConfigMgr& getGlobalConfig(); // Will be in fixture_base
```

Actually, let me redesign this to not depend on external helpers. The AccountManager needs config access. Let me use the existing ConfigMgr singleton.

`account_manager.cpp`:

```cpp
#include "account_manager.h"
#include "http_test_client.h"
#include "ConfigMgr.h"
#include "Singleton.h"
#include "RedisMgr.h"
#include <iostream>
#include <chrono>

AccountManager::AccountManager() {
    auto& config = ConfigMgr::getInstance();
    gateHost_ = config["GateServer"]["Host"].asString();
    if (gateHost_.empty() || gateHost_ == "127.0.0.1") {
        gateHost_ = "127.0.0.1";
    }
    gatePort_ = static_cast<uint16_t>(
        std::stoi(config["GateServer"]["Port"]));
}

AccountManager::~AccountManager() {
    releaseAll();
}

std::string AccountManager::fetchVerifyCode(const std::string& email) {
    // Connect to Redis to fetch verify code (set by GateServer during GetVerifyCode)
    // Use existing Redis pool
    auto redis = RedisMgr::getInstance();
    std::string key = CODE_PREFIX + email;
    return redis->get(key);
}

TestAccount AccountManager::registerAccount(const std::string& email) {
    TestAccount acct;
    acct.email = email;

    HttpTestClient http(gateHost_, gatePort_);

    // 1. Request verify code
    Json::Value verifyReq;
    verifyReq["email"] = email;
    auto verifyRsp = http.post("/get_verify_code", verifyReq);

    // 2. Get code from Redis
    std::string code = fetchVerifyCode(email);

    // 3. Register
    Json::Value regReq;
    regReq["email"] = email;
    regReq["nick"] = email.substr(0, email.find('@'));
    regReq["pwd"] = "Test123456";
    regReq["verifycode"] = code;
    auto regRsp = http.post("/reg_user", regReq);

    acct.uid = regRsp.body["uid"].asInt();
    return acct;
}

TestAccount AccountManager::acquire(const std::string& tag) {
    std::string email = "test_" + tag + "_" + std::to_string(++userCount) + "@test.com";
    TestAccount acct = registerAccount(email);
    heldUids_.insert(acct.uid);
    return acct;
}

std::vector<TestAccount> AccountManager::acquireBatch(int n, const std::string& tag) {
    std::vector<TestAccount> accounts;
    for (int i = 0; i < n; i++) {
        accounts.push_back(acquire(tag + "_" + std::to_string(i)));
    }
    return accounts;
}

void AccountManager::release(int uid) {
    heldUids_.erase(uid);
    // Delete user from DB directly via MySQL for cleanup
    auto sql = MysqlPool::getInstance();
    auto con = sql->getConnection();
    if (con) {
        // Soft delete: remove user records
        try {
            con->setSchema("IMServer");
            std::unique_ptr<sql::Statement> stmt(con->createStatement());
            stmt->execute("DELETE FROM user_base_info WHERE uid = " + std::to_string(uid));
        } catch (sql::SQLException& e) {
            std::cerr << "AccountManager release error: " << e.what() << std::endl;
        }
    }
}

void AccountManager::releaseAll() {
    for (int uid : heldUids_) {
        release(uid);
    }
    heldUids_.clear();
}
```

- [ ] **Step 3: Write fixture_base.h**

```cpp
#ifndef IMSERVER_FIXTURE_BASE_H
#define IMSERVER_FIXTURE_BASE_H

#include <gtest/gtest.h>
#include "account_manager.h"
#include "ConfigMgr.h"

class IntegrationTestBase : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        gSkip = false;
        try {
            auto& config = ConfigMgr::getInstance();
            // Test Redis reachability
            auto redis = RedisMgr::getInstance();
            redis->set("__test_ping__", "1");
            std::string val = redis->get("__test_ping__");
            if (val != "1") {
                gSkip = true;
                std::cerr << "[IntegrationTestBase] Redis unreachable, skipping integration tests" << std::endl;
            }
        } catch (const std::exception& e) {
            gSkip = true;
            std::cerr << "[IntegrationTestBase] External dependency check failed: " << e.what() << std::endl;
        }
    }

    void SetUp() override {
        if (gSkip) {
            GTEST_SKIP() << "External dependencies unavailable (Redis/MySQL not reachable)";
        }
        accountMgr_ = std::make_unique<AccountManager>();
    }

    void TearDown() override {
        if (accountMgr_) {
            accountMgr_->releaseAll();
        }
    }

    static inline bool gSkip = false;
    std::unique_ptr<AccountManager> accountMgr_;
};

#endif
```

- [ ] **Step 4: Update test/CMakeLists.txt**

`account_manager.cpp` already in `test_framework` sources. Ensure `hiredis_deps` and `mysqlcppconn_deps` are linked to `test_framework`.

- [ ] **Step 5: Commit**

```bash
git add test/framework/fixture_base.h test/framework/account_manager.h test/framework/account_manager.cpp
git commit -m "test: add IntegrationTestBase fixture and AccountManager for data isolation"
```

---

### Task 6: GateServer 集成测试

**Files:**
- Create: `test/gate/gate_integration_test.cpp`

**Interfaces:**
- Consumes: `HttpTestClient`, `IntegrationTestBase`, `AccountManager`
- Consumes: GateServer HTTP endpoints (running on `config.ini` port)

- [ ] **Step 1: Write gate_integration_test.cpp**

```cpp
#include <gtest/gtest.h>
#include "fixture_base.h"
#include "http_test_client.h"
#include "ConfigMgr.h"

class GateIntegrationTest : public IntegrationTestBase {
protected:
    void SetUp() override {
        IntegrationTestBase::SetUp();
        auto& config = ConfigMgr::getInstance();
        std::string host = "127.0.0.1";
        uint16_t port = static_cast<uint16_t>(std::stoi(config["GateServer"]["Port"]));
        http_ = std::make_unique<HttpTestClient>(host, port);
    }

    std::unique_ptr<HttpTestClient> http_;
};

TEST_F(GateIntegrationTest, GetVerifyCodeSuccess) {
    Json::Value req;
    req["email"] = "gate_int_verify@test.com";
    auto rsp = http_->post("/get_verify_code", req);
    EXPECT_EQ(rsp.code, 200);
    EXPECT_EQ(rsp.body["error"].asInt(), 0);
}

TEST_F(GateIntegrationTest, RegisterNewUser) {
    auto acct = accountMgr_->acquire("reg");
    EXPECT_GT(acct.uid, 0);
    EXPECT_NE(acct.email.find("test_reg_"), std::string::npos);
}

TEST_F(GateIntegrationTest, RegisterDuplicateFails) {
    auto acct = accountMgr_->acquire("dup");
    // Try registering same email again
    Json::Value req;
    req["email"] = acct.email;
    req["nick"] = "dup_user";
    req["pwd"] = "Test123456";
    req["verifycode"] = "000000"; // wrong code
    auto rsp = http_->post("/reg_user", req);
    EXPECT_NE(rsp.body["error"].asInt(), 0); // should fail (wrong code)
}
```

- [ ] **Step 2: Update test/CMakeLists.txt**

`gate_integration_test.cpp` is already in `IMTest` sources (Task 1).

- [ ] **Step 3: Build and run**

```bash
cd cmake-build-debug && ninja IMTest
./bin/IMTest --gtest_filter="GateIntegrationTest.*"
```
Expected: 3 tests pass (requires GateServer running + Redis)

- [ ] **Step 4: Commit**

```bash
git add test/gate/gate_integration_test.cpp
git commit -m "test: add GateServer integration tests for register and verify code flows"
```

---

### Task 7: StatusServer 集成测试

**Files:**
- Create: `test/status/status_integration_test.cpp`

**Interfaces:**
- Consumes: `GrpcTestClient`, `IntegrationTestBase`, proto StatusService

- [ ] **Step 1: Write status_integration_test.cpp**

```cpp
#include <gtest/gtest.h>
#include "fixture_base.h"
#include "grpc_test_client.h"
#include "message.grpc.pb.h"
#include "ConfigMgr.h"

using message::GetChatServerReq;
using message::GetChatServerRsp;
using message::LoginReq;
using message::LoginRsp;
using message::StatusService;

class StatusIntegrationTest : public IntegrationTestBase {
protected:
    void SetUp() override {
        IntegrationTestBase::SetUp();
        auto& config = ConfigMgr::getInstance();
        std::string host = "127.0.0.1";
        uint16_t port = static_cast<uint16_t>(std::stoi(config["StatusServer"]["Port"]));
        grpc_ = std::make_unique<GrpcTestClient<StatusService>>(host, port);
    }

    std::unique_ptr<GrpcTestClient<StatusService>> grpc_;
};

TEST_F(StatusIntegrationTest, GetChatServerReturnsHostAndToken) {
    GetChatServerReq req;
    req.set_uid(1001);
    GetChatServerRsp rsp;
    grpc::ClientContext ctx;

    auto status = grpc_->stub()->GetChatServer(&ctx, req, &rsp);
    EXPECT_TRUE(status.ok());
    EXPECT_FALSE(rsp.host().empty());
    EXPECT_GT(rsp.token().size(), 0);
}

TEST_F(StatusIntegrationTest, LoginAndVerifyToken) {
    // First get a chat server assignment
    GetChatServerReq gsReq;
    gsReq.set_uid(2002);
    GetChatServerRsp gsRsp;
    grpc::ClientContext gsCtx;
    auto gsStatus = grpc_->stub()->GetChatServer(&gsCtx, gsReq, &gsRsp);
    ASSERT_TRUE(gsStatus.ok());

    // Login
    LoginReq loginReq;
    loginReq.set_uid(2002);
    loginReq.set_token(gsRsp.token());
    LoginRsp loginRsp;
    grpc::ClientContext loginCtx;
    auto loginStatus = grpc_->stub()->Login(&loginCtx, loginReq, &loginRsp);
    EXPECT_TRUE(loginStatus.ok());
    EXPECT_EQ(loginRsp.uid(), 2002);
}
```

- [ ] **Step 2: Build and run**

```bash
cd cmake-build-debug && ninja IMTest
./bin/IMTest --gtest_filter="StatusIntegrationTest.*"
```
Expected: 2 tests pass (requires StatusServer running)

- [ ] **Step 3: Commit**

```bash
git add test/status/status_integration_test.cpp
git commit -m "test: add StatusServer integration tests for GetChatServer and Login"
```

---

### Task 8: 分层运行脚本 + 标签体系

**Files:**
- Create: `test/run_tests.sh`

**Interfaces:**
- Produces: script to run tests by tag

- [ ] **Step 1: Write run_tests.sh**

```bash
#!/bin/bash
# 分层测试运行脚本
# 用法:
#   ./run_tests.sh unit          # 只跑单元测试
#   ./run_tests.sh integration   # 只跑集成测试
#   ./run_tests.sh all           # 全部（默认）

set -e

IMTEST="./cmake-build-debug/bin/IMTest"
TAG="${1:-all}"

case "$TAG" in
    unit)
        FILTER="*:-*[integration]*:-*[stability]*:-*[perf]*"
        ;;
    integration)
        FILTER="*[integration]*"
        ;;
    all)
        FILTER="*"
        ;;
    *)
        FILTER="*"
        ;;
esac

echo "=== Running tests with filter: $FILTER ==="
"$IMTEST" --gtest_filter="$FILTER" --gtest_color=yes
```

- [ ] **Step 2: Make executable**

```bash
chmod +x test/run_tests.sh
```

- [ ] **Step 3: Commit**

```bash
git add test/run_tests.sh
git commit -m "test: add layered test runner script (unit/integration/all)"
```

---

## Phase 2: 稳定性长跑（Week 3-4）

### Task 9: ChatTestClient

**Files:**
- Create: `test/framework/chat_test_client.h`
- Create: `test/framework/chat_test_client.cpp`
- Create: `test/chat/chat_client_test.cpp`

**Interfaces:**
- Produces: `ChatTestClient` with `connect`, `disconnect`, `isConnected`, `send`, `sendAndWait`, `onMessage`, `chatLogin`, `sendChatMsg`, `heartbeat`
- Produces: message counters (`messagesSent()`, `messagesReceived()`, `errors()`)

- [ ] **Step 1: Write chat_test_client.h**

```cpp
#ifndef IMSERVER_CHAT_TEST_CLIENT_H
#define IMSERVER_CHAT_TEST_CLIENT_H

#include <cstdint>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <atomic>
#include <boost/asio.hpp>
#include <json/json.h>

namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

struct PendingResponse {
    std::function<bool(uint16_t, const Json::Value&)> matcher;
    std::promise<std::optional<Json::Value>> promise;
};

class ChatTestClient {
public:
    using MessageHandler = std::function<void(uint16_t msgId, const Json::Value& body)>;

    ChatTestClient();
    ~ChatTestClient();

    bool connect(const std::string& host, uint16_t port, int timeoutMs = 5000);
    void disconnect();
    bool isConnected() const;

    std::optional<Json::Value> sendAndWait(
        uint16_t msgId, const Json::Value& body,
        std::function<bool(uint16_t, const Json::Value&)> match,
        int timeoutMs = 5000);

    bool send(uint16_t msgId, const Json::Value& body);
    void onMessage(uint16_t msgId, MessageHandler handler);

    bool chatLogin(int uid, const std::string& token);
    bool sendChatMsg(int toUid, const std::string& content);
    bool heartbeat();

    uint64_t messagesSent() const { return sent_; }
    uint64_t messagesReceived() const { return recv_; }
    uint64_t errors() const { return errors_; }

private:
    void recvLoop();
    void handleFrame(uint16_t msgId, const Json::Value& body);

    std::atomic<bool> connected_{false};
    std::atomic<bool> stop_{false};
    net::io_context ioc_;
    std::unique_ptr<tcp::socket> socket_;
    std::unique_ptr<std::thread> recvThread_;
    std::string recvBuffer_;

    std::mutex pendingMtx_;
    std::queue<std::shared_ptr<PendingResponse>> pending_;

    std::mutex handlerMtx_;
    std::unordered_map<uint16_t, MessageHandler> handlers_;

    std::atomic<uint64_t> sent_{0};
    std::atomic<uint64_t> recv_{0};
    std::atomic<uint64_t> errors_{0};
};

#endif
```

- [ ] **Step 2: Write chat_test_client.cpp**

```cpp
#include "chat_test_client.h"
#include "protocol.h"
#include <iostream>

ChatTestClient::ChatTestClient() = default;

ChatTestClient::~ChatTestClient() {
    disconnect();
}

bool ChatTestClient::connect(const std::string& host, uint16_t port, int timeoutMs) {
    try {
        tcp::resolver resolver(ioc_);
        auto endpoints = resolver.resolve(host, std::to_string(port));
        socket_ = std::make_unique<tcp::socket>(ioc_);

        boost::system::error_code ec;
        boost::asio::connect(*socket_, endpoints, ec);
        if (ec) {
            errors_++;
            return false;
        }

        connected_ = true;
        stop_ = false;
        recvThread_ = std::make_unique<std::thread>(&ChatTestClient::recvLoop, this);
        return true;
    } catch (const std::exception& e) {
        errors_++;
        return false;
    }
}

void ChatTestClient::disconnect() {
    stop_ = true;
    connected_ = false;
    if (socket_ && socket_->is_open()) {
        boost::system::error_code ec;
        socket_->close(ec);
    }
    if (recvThread_ && recvThread_->joinable()) {
        recvThread_->join();
    }
}

bool ChatTestClient::isConnected() const {
    return connected_;
}

bool ChatTestClient::send(uint16_t msgId, const Json::Value& body) {
    if (!connected_) return false;
    std::string frame = encode(msgId, body);
    boost::system::error_code ec;
    boost::asio::write(*socket_, boost::asio::buffer(frame), ec);
    if (ec) {
        errors_++;
        connected_ = false;
        return false;
    }
    sent_++;
    return true;
}

std::optional<Json::Value> ChatTestClient::sendAndWait(
    uint16_t msgId, const Json::Value& body,
    std::function<bool(uint16_t, const Json::Value&)> match,
    int timeoutMs) {

    auto pending = std::make_shared<PendingResponse>();
    pending->matcher = match;
    auto future = pending->promise.get_future();

    {
        std::lock_guard<std::mutex> lock(pendingMtx_);
        pending_.push(pending);
    }

    if (!send(msgId, body)) {
        std::lock_guard<std::mutex> lock(pendingMtx_);
        pending_.pop();
        return std::nullopt;
    }

    if (future.wait_for(std::chrono::milliseconds(timeoutMs)) == std::future_status::timeout) {
        errors_++;
        return std::nullopt;
    }
    return future.get();
}

void ChatTestClient::onMessage(uint16_t msgId, MessageHandler handler) {
    std::lock_guard<std::mutex> lock(handlerMtx_);
    handlers_[msgId] = handler;
}

bool ChatTestClient::chatLogin(int uid, const std::string& token) {
    Json::Value body;
    body["uid"] = uid;
    body["token"] = token;
    auto rsp = sendAndWait(1005, body, // ID_CHAT_LOGIN = 1005
        [](uint16_t id, const Json::Value&) { return id == 1006; }); // ID_CHAT_LOGIN_RSP
    return rsp.has_value() && rsp->isMember("error") && (*rsp)["error"].asInt() == 0;
}

bool ChatTestClient::sendChatMsg(int toUid, const std::string& content) {
    Json::Value body;
    body["touid"] = toUid;
    body["msg"] = content;
    body["msgid"] = 1; // text
    auto rsp = sendAndWait(3001, body, // ID_CHAT_MSG_REQ
        [](uint16_t id, const Json::Value&) { return id == 3002; }); // ID_CHAT_MSG_RSP
    return rsp.has_value() && (*rsp)["error"].asInt() == 0;
}

bool ChatTestClient::heartbeat() {
    Json::Value body;
    auto rsp = sendAndWait(5002, body, // ID_HEART_BEAT_REQ
        [](uint16_t id, const Json::Value&) { return id == 5003; }); // ID_HEART_BEAT_RSP
    return rsp.has_value();
}

void ChatTestClient::recvLoop() {
    char buf[4096];
    while (!stop_) {
        boost::system::error_code ec;
        size_t n = socket_->read_some(boost::asio::buffer(buf, sizeof(buf)), ec);
        if (ec) {
            connected_ = false;
            errors_++;
            break;
        }
        auto frames = decode(buf, n);
        for (auto& f : frames) {
            handleFrame(f.msgId, f.body);
        }
    }
}

void ChatTestClient::handleFrame(uint16_t msgId, const Json::Value& body) {
    recv_++;

    // Check pending requests
    {
        std::lock_guard<std::mutex> lock(pendingMtx_);
        if (!pending_.empty()) {
            auto& front = pending_.front();
            if (front->matcher(msgId, body)) {
                front->promise.set_value(body);
                pending_.pop();
                return;
            }
        }
    }

    // Check registered handlers
    {
        std::lock_guard<std::mutex> lock(handlerMtx_);
        auto it = handlers_.find(msgId);
        if (it != handlers_.end()) {
            it->second(msgId, body);
        }
    }
}
```

- [ ] **Step 3: Write chat_client_test.cpp**

```cpp
#include <gtest/gtest.h>
#include "fixture_base.h"
#include "chat_test_client.h"
#include "ConfigMgr.h"
#include "grpc_test_client.h"

class ChatClientTest : public IntegrationTestBase {
protected:
    void SetUp() override {
        IntegrationTestBase::SetUp();
        auto& config = ConfigMgr::getInstance();
        chatHost_ = "127.0.0.1";
        chatPort_ = static_cast<uint16_t>(std::stoi(config["ChatServer"]["Port"]));
    }

    std::string chatHost_;
    uint16_t chatPort_;
};

TEST_F(ChatClientTest, ConnectDisconnect) {
    ChatTestClient client;
    EXPECT_TRUE(client.connect(chatHost_, chatPort_));
    EXPECT_TRUE(client.isConnected());
    client.disconnect();
    EXPECT_FALSE(client.isConnected());
}

TEST_F(ChatClientTest, InvalidConnectFails) {
    ChatTestClient client;
    EXPECT_FALSE(client.connect("127.0.0.1", 19999)); // no server
}

TEST_F(ChatClientTest, Heartbeat) {
    ChatTestClient client;
    ASSERT_TRUE(client.connect(chatHost_, chatPort_));
    // Heartbeat without login may fail server-side but should get a response
    client.heartbeat();
    EXPECT_EQ(client.messagesSent(), 1u);
}
```

- [ ] **Step 4: Update test/CMakeLists.txt**

Add `chat_test_client.cpp` to `test_framework` sources and `chat_client_test.cpp` to `IMTest`.

- [ ] **Step 5: Build and run**

```bash
cd cmake-build-debug && ninja IMTest
./bin/IMTest --gtest_filter="ChatClientTest.*"
```
Expected: tests pass (requires ChatServer running)

- [ ] **Step 6: Commit**

```bash
git add test/framework/chat_test_client.h test/framework/chat_test_client.cpp test/chat/chat_client_test.cpp
git commit -m "test: add ChatTestClient with TCP protocol and basic connection tests"
```

---

### Task 10: ChatServer 集成测试

**Files:**
- Create: `test/chat/chat_integration_test.cpp`

**Interfaces:**
- Consumes: `ChatTestClient`, `IntegrationTestBase`, proto StatusService (for chat server assignment)

- [ ] **Step 1: Write chat_integration_test.cpp**

```cpp
#include <gtest/gtest.h>
#include "fixture_base.h"
#include "chat_test_client.h"
#include "grpc_test_client.h"
#include "message.grpc.pb.h"
#include "ConfigMgr.h"

using message::GetChatServerReq;
using message::GetChatServerRsp;
using message::StatusService;

class ChatIntegrationTest : public IntegrationTestBase {
protected:
    void SetUp() override {
        IntegrationTestBase::SetUp();
        auto& config = ConfigMgr::getInstance();
        chatHost_ = "127.0.0.1";
        chatPort_ = static_cast<uint16_t>(std::stoi(config["ChatServer"]["Port"]));
    }

    std::string chatHost_;
    uint16_t chatPort_;
};

TEST_F(ChatIntegrationTest, LoginWithToken) {
    // Get valid token from StatusServer
    auto& config = ConfigMgr::getInstance();
    GrpcTestClient<StatusService> statusCli("127.0.0.1",
        config["StatusServer"]["Port"]);

    GetChatServerReq gsReq;
    gsReq.set_uid(3001);
    GetChatServerRsp gsRsp;
    grpc::ClientContext gsCtx;
    ASSERT_TRUE(statusCli.stub()->GetChatServer(&gsCtx, gsReq, &gsRsp).ok());

    // Login to ChatServer
    ChatTestClient chatCli;
    ASSERT_TRUE(chatCli.connect(chatHost_, chatPort_));
    EXPECT_TRUE(chatCli.chatLogin(3001, gsRsp.token()));
}

TEST_F(ChatIntegrationTest, SendMessageToOffline) {
    // Login user A
    auto acct = accountMgr_->acquire("chat_send");
    // ... get token, login, send to non-existent user, verify error received
}
```

- [ ] **Step 2: Build and run**

```bash
cd cmake-build-debug && ninja IMTest
./bin/IMTest --gtest_filter="ChatIntegrationTest.*"
```
Expected: tests pass (requires GateServer + StatusServer + ChatServer)

- [ ] **Step 3: Commit**

```bash
git add test/chat/chat_integration_test.cpp
git commit -m "test: add ChatServer integration tests for login and messaging"
```

---

### Task 11: Metrics 库

**Files:**
- Create: `test/framework/metrics.h`
- Create: `test/framework/metrics.cpp`

**Interfaces:**
- Produces: `LatencyHistogram` (record, percentile, min/max/avg)
- Produces: `ThroughputCounter` (tick, total)
- Produces: `ErrorCounter` (addFailed/addSuccess, errorRate)
- Produces: `Metrics` (summary(), toJson())

- [ ] **Step 1: Write metrics.h**

```cpp
#ifndef IMSERVER_METRICS_H
#define IMSERVER_METRICS_H

#include <cstdint>
#include <chrono>
#include <vector>
#include <algorithm>
#include <numeric>
#include <string>
#include <json/json.h>

class LatencyHistogram {
public:
    void record(int64_t us);
    int64_t percentile(double p) const;
    int64_t min() const;
    int64_t max() const;
    int64_t avg() const;
    uint64_t count() const { return count_; }

private:
    std::vector<int64_t> samples_;
    uint64_t count_ = 0;
    int64_t min_ = INT64_MAX;
    int64_t max_ = 0;
    int64_t sum_ = 0;
};

class ThroughputCounter {
public:
    void tick();
    uint64_t total() const { return total_; }
private:
    std::atomic<uint64_t> total_{0};
};

class ErrorCounter {
public:
    void addFailed();
    void addSuccess();
    uint64_t failed() const { return failed_; }
    uint64_t success() const { return success_; }
    double errorRate() const;

private:
    std::atomic<uint64_t> failed_{0};
    std::atomic<uint64_t> success_{0};
};

struct Metrics {
    LatencyHistogram latency;
    ThroughputCounter throughput;
    ErrorCounter errors;
    std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();

    std::string summary() const;
    Json::Value toJson() const;
};

#endif
```

- [ ] **Step 2: Write metrics.cpp**

```cpp
#include "metrics.h"
#include <sstream>
#include <iomanip>

void LatencyHistogram::record(int64_t us) {
    samples_.push_back(us);
    count_++;
    min_ = std::min(min_, us);
    max_ = std::max(max_, us);
    sum_ += us;
}

int64_t LatencyHistogram::percentile(double p) const {
    if (samples_.empty()) return 0;
    auto sorted = samples_;
    std::sort(sorted.begin(), sorted.end());
    size_t idx = static_cast<size_t>(p * sorted.size());
    if (idx >= sorted.size()) idx = sorted.size() - 1;
    return sorted[idx];
}

int64_t LatencyHistogram::min() const { return samples_.empty() ? 0 : min_; }
int64_t LatencyHistogram::max() const { return samples_.empty() ? 0 : max_; }
int64_t LatencyHistogram::avg() const { return count_ ? sum_ / count_ : 0; }

void ThroughputCounter::tick() { total_++; }

void ErrorCounter::addFailed() { failed_++; }
void ErrorCounter::addSuccess() { success_++; }

double ErrorCounter::errorRate() const {
    uint64_t total = success_ + failed_;
    return total == 0 ? 0.0 : static_cast<double>(failed_) / total;
}

std::string Metrics::summary() const {
    std::ostringstream oss;
    oss << "count=" << latency.count()
        << " qps=" << std::fixed << std::setprecision(1)
        << (latency.count() /
            std::max(1.0, std::chrono::duration<double>(
                std::chrono::steady_clock::now() - startTime).count()))
        << " p50=" << latency.percentile(0.5) << "us"
        << " p99=" << latency.percentile(0.99) << "us"
        << " err=" << std::setprecision(3) << errors.errorRate() * 100 << "%";
    return oss.str();
}

Json::Value Metrics::toJson() const {
    Json::Value root;
    root["count"] = static_cast<Json::UInt64>(latency.count());
    root["p50_us"] = latency.percentile(0.5);
    root["p99_us"] = latency.percentile(0.99);
    root["avg_us"] = latency.avg();
    root["errors"] = static_cast<Json::UInt64>(errors.failed());
    root["error_rate"] = errors.errorRate();
    return root;
}
```

- [ ] **Step 3: Update test/CMakeLists.txt**

Add `framework/metrics.cpp` to `test_framework` sources.

- [ ] **Step 4: Build**

```bash
cd cmake-build-debug && ninja IMTest
```
Expected: compiles

- [ ] **Step 5: Commit**

```bash
git add test/framework/metrics.h test/framework/metrics.cpp
git commit -m "test: add metrics library (latency histogram, throughput, error rate)"
```

---

### Task 12: Resource Monitor

**Files:**
- Create: `test/framework/resource_monitor.h`

**Interfaces:**
- Produces: `ResourceMonitor` with `sample() -> ResourceSnapshot`, `isLeaking()`

- [ ] **Step 1: Write resource_monitor.h**

```cpp
#ifndef IMSERVER_RESOURCE_MONITOR_H
#define IMSERVER_RESOURCE_MONITOR_H

#include <unistd.h>
#include <fstream>
#include <string>

struct ResourceSnapshot {
    int   fdCount = 0;
    long  vmRSS_KB = 0;
    int   threadCount = 0;
};

class ResourceMonitor {
public:
    ResourceSnapshot sample() const {
        ResourceSnapshot snap;
        // fd count
        snap.fdCount = 0;
        for (auto& p : std::filesystem::directory_iterator("/proc/self/fd")) {
            (void)p;
            snap.fdCount++;
        }
        // VmRSS
        std::ifstream status("/proc/self/status");
        std::string line;
        while (std::getline(status, line)) {
            if (line.find("VmRSS:") == 0) {
                snap.vmRSS_KB = std::stol(line.substr(7));
                break;
            }
        }
        return snap;
    }

    bool isLeaking(const ResourceSnapshot& baseline,
                   const ResourceSnapshot& current) const {
        if (baseline.fdCount > 0 &&
            current.fdCount > baseline.fdCount * 1.2) return true;
        if (baseline.vmRSS_KB > 0 &&
            current.vmRSS_KB > baseline.vmRSS_KB * 1.3) return true;
        return false;
    }
};

#endif
```

- [ ] **Step 2: Commit**

```bash
git add test/framework/resource_monitor.h
git commit -m "test: add resource monitor for fd/RSS leak detection"
```

---

### Task 13: Report Writer

**Files:**
- Create: `test/framework/report.h`
- Create: `test/framework/report.cpp`

**Interfaces:**
- Produces: `ReportWriter` with `appendSample(timestamp, metrics)`, `writeSummary(path)`, `writeDataFile(path)`

- [ ] **Step 1: Write report.h**

```cpp
#ifndef IMSERVER_REPORT_H
#define IMSERVER_REPORT_H

#include "metrics.h"
#include <string>
#include <vector>

struct SampleRecord {
    time_t timestamp;
    Json::Value metrics;
};

class ReportWriter {
public:
    void appendSample(time_t t, const Metrics& m);
    void writeSummary(const std::string& path) const;
    void writeDataFile(const std::string& path) const;

private:
    std::vector<SampleRecord> samples_;
};

#endif
```

- [ ] **Step 2: Write report.cpp**

```cpp
#include "report.h"
#include <fstream>
#include <iostream>
#include <iomanip>

void ReportWriter::appendSample(time_t t, const Metrics& m) {
    samples_.push_back({t, m.toJson()});
}

void ReportWriter::writeSummary(const std::string& path) const {
    std::ofstream out(path);
    out << "=== Test Report ===\n";
    for (const auto& s : samples_) {
        out << "t=" << s.timestamp
            << " " << s.metrics.toStyledString();
    }
}

void ReportWriter::writeDataFile(const std::string& path) const {
    std::ofstream out(path);
    out << "timestamp,count,p50_us,p99_us,avg_us,errors,error_rate\n";
    for (const auto& s : samples_) {
        out << s.timestamp << ","
            << s.metrics["count"].asUInt64() << ","
            << s.metrics["p50_us"].asInt64() << ","
            << s.metrics["p99_us"].asInt64() << ","
            << s.metrics["avg_us"].asInt64() << ","
            << s.metrics["errors"].asUInt64() << ","
            << std::fixed << std::setprecision(4)
            << s.metrics["error_rate"].asDouble() << "\n";
    }
}
```

- [ ] **Step 3: Commit**

```bash
git add test/framework/report.h test/framework/report.cpp
git commit -m "test: add report writer for summary and CSV time-series output"
```

---

### Task 14: Stability Runner

**Files:**
- Create: `test/framework/stability_runner.h`
- Create: `test/framework/stability_runner.cpp`
- Create: `test/integration/stability_test.cpp`

**Interfaces:**
- Produces: `StabilityRunner` with `runKeepAlive`, `runChurn`, `runMessageStorm`, `runMixed`
- Produces: `stability_test.cpp` with gtest cases tagged `[stability]`

- [ ] **Step 1: Write stability_runner.h**

```cpp
#ifndef IMSERVER_STABILITY_RUNNER_H
#define IMSERVER_STABILITY_RUNNER_H

#include "metrics.h"
#include "resource_monitor.h"
#include "report.h"
#include <functional>
#include <string>

struct StabilityConfig {
    std::string chatHost;
    uint16_t chatPort;
    int clientCount = 10;
    int durationSec = 3600;
    std::chrono::seconds heartbeatInterval{30};
    std::chrono::seconds onlineSec{60};
    std::chrono::seconds offlineSec{10};
    int msgPerClientPerSec = 1;
    std::string reportPath = "stability_report.csv";
};

class StabilityRunner {
public:
    using ClientFactory = std::function<void(int /*clientIdx*/)>;

    explicit StabilityRunner(StabilityConfig config);

    // Run a single scenario, returns true if all pass criteria met
    bool runKeepAlive();
    bool runChurn();
    bool runMessageStorm();
    bool runMixed();

    const Metrics& metrics() const { return metrics_; }
    const ReportWriter& report() const { return report_; }

private:
    StabilityConfig config_;
    Metrics metrics_;
    ReportWriter report_;
    ResourceMonitor resourceMon_;
};

#endif
```

- [ ] **Step 2: Write stability_runner.cpp**

Implement the four scenarios. Each runs for `durationSec`, sampling metrics every 30s, appending to report. Leak check via `ResourceMonitor` at start/pass.

- [ ] **Step 3: Write stability_test.cpp**

```cpp
#include "fixture_base.h"
#include "stability_runner.h"
#include "ConfigMgr.h"

class StabilityTest : public IntegrationTestBase {};

TEST_F(StabilityTest, [stability] KeepAlive30Min) {
    auto& config = ConfigMgr::getInstance();
    StabilityConfig cfg;
    cfg.chatHost = "127.0.0.1";
    cfg.chatPort = static_cast<uint16_t>(std::stoi(config["ChatServer"]["Port"]));
    cfg.clientCount = 5;
    cfg.durationSec = 1800; // 30 min for CI; use 86400 for full 24h

    StabilityRunner runner(cfg);
    EXPECT_TRUE(runner.runKeepAlive());
}

TEST_F(StabilityTest, [stability] Mixed1Hour) {
    auto& config = ConfigMgr::getInstance();
    StabilityConfig cfg;
    cfg.chatHost = "127.0.0.1";
    cfg.chatPort = static_cast<uint16_t>(std::stoi(config["ChatServer"]["Port"]));
    cfg.clientCount = 10;
    cfg.durationSec = 3600;

    StabilityRunner runner(cfg);
    EXPECT_TRUE(runner.runMixed());
    runner.report().writeDataFile(cfg.reportPath);
}
```

- [ ] **Step 4: Commit** (all four files)

---

## Phase 3: 性能压测（Week 5+）

### Task 15: PerfSuite

**Files:**
- Create: `test/perf/perf_suite.h`
- Create: `test/perf/perf_suite.cpp`
- Create: `test/perf/chat_perf_test.cpp`

**Interfaces:**
- Produces: `PerfSuite` with `runRampUp()`, `runToBreak()`, `runMixedWorkload()`
- Produces: `chat_perf_test.cpp` with gtest cases tagged `[perf]`

(Follows the same pattern as stability runner but with varying client counts and no long waits.)

---

### Task 16: Baseline Card + Profiler Integration

**Files:**
- Create: `test/perf/baseline_card.h`
- Create: `scripts/run_perf.sh`

**Interfaces:**
- Produces: baseline card text output
- Produces: shell script that runs `perf record` / `heaptrack` alongside `IMTest --gtest_filter="*[perf]*"`

---

### Task 17: CI 集成

**Files:**
- Create: `.github/workflows/test.yml` (or equivalent CI config)

**Interfaces:**
- Produces: PR pipeline runs `unit`, merge pipeline runs `integration` + short `stability`

---

## File-to-Task Mapping（总览）

| 文件 | 创建/修改 | 所在 Task |
|------|---------|----------|
| `test/CMakeLists.txt` | 修改 | 1, 2, 3, 9, 10, 11 |
| `CMakeLists.txt` (main) | 修改 | 1 |
| `test/framework/protocol.h/.cpp` | 创建 | 2 |
| `test/framework/protocol_test.cpp` | 创建 | 2 |
| `test/framework/http_test_client.h/.cpp` | 创建 | 3 |
| `test/framework/grpc_test_client.h` | 创建 | 4 |
| `test/framework/fixture_base.h` | 创建 | 5 |
| `test/framework/account_manager.h/.cpp` | 创建 | 5 |
| `test/gate/gate_integration_test.cpp` | 创建 | 6 |
| `test/status/status_integration_test.cpp` | 创建 | 7 |
| `test/run_tests.sh` | 创建 | 8 |
| `test/framework/chat_test_client.h/.cpp` | 创建 | 9 |
| `test/chat/chat_client_test.cpp` | 创建 | 9 |
| `test/chat/chat_integration_test.cpp` | 创建 | 10 |
| `test/framework/metrics.h/.cpp` | 创建 | 11 |
| `test/framework/resource_monitor.h` | 创建 | 12 |
| `test/framework/report.h/.cpp` | 创建 | 13 |
| `test/framework/stability_runner.h/.cpp` | 创建 | 14 |
| `test/integration/stability_test.cpp` | 创建 | 14 |
| `test/perf/perf_suite.h/.cpp` | 创建 | 15 |
| `test/perf/chat_perf_test.cpp` | 创建 | 15 |
| `test/perf/baseline_card.h` | 创建 | 16 |
| `scripts/run_perf.sh` | 创建 | 16 |
| `.github/workflows/test.yml` | 创建 | 17 |
