//
// Created by Fan on 2026/4/30.
//

#ifndef IMSERVER_CONST_H
#define IMSERVER_CONST_H

#include <utility>
#include <functional>

#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/asio.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

constexpr int DEFAULT_RPC_POOL_SIZE = 10;
constexpr int DEFAULT_REDIS_POOL_SIZE = 5;
constexpr int DEFAULT_MYSQL_POOL_SIZE = 5;

constexpr int HEAD_TOTAL_LEN = 4;
constexpr int HEAD_MSG_ID_LEN = 2;
constexpr int HEAD_MSG_SIZE_LEN = 2;
constexpr int MAX_BUFFER_SIZE = 8196;

#define CODE_PREFIX "code_"

enum class ErrorCodes : int32_t {
    SUCCESS = 0,
    ERROR_JSON = 1001,
    RPC_FAILED = 1002,
    VERIFY_CODE_EXPIRED = 1003,
    VERIFY_CODE_NOT_REACHED = 1004,
    USER_EXISTS = 1005,
    USER_EMAIL_NOT_EXISTS = 1006,
};

enum class MessageID : uint16_t {
    CHAT_LOGIN = 2001,
    CHAT_LOGIN_RSP = 2002,
    INVALID_ID,
};

struct UserInfo {
    std::string name;
    std::string email;
    std::string password;
    int uid = 0;
};

class Defer {
public:
    explicit Defer(std::function<void()> func) : func_(std::move(func)) {};
    ~Defer() {
        func_();
    }

private:
    std::function<void()> func_;
};

#endif //IMSERVER_CONST_H