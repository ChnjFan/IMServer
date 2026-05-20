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

#define USER_IP_PREFIX "user_ip_"
#define USER_TOKEN_PREFIX "user_token_"
#define IP_COUNT_PREFIX "ip_count_"
#define USER_BASE_INFO_PREFIX "user_base_info_"
#define LOGIN_COUNT "login_chat_server_count"

enum class ErrorCodes : int32_t {
    SUCCESS = 0,
    ERROR_JSON = 1001,
    RPC_FAILED = 1002,
    VERIFY_CODE_EXPIRED = 1003,
    VERIFY_CODE_NOT_REACHED = 1004,
    USER_EXISTS = 1005,
    USER_EMAIL_NOT_EXISTS = 1006,
    CHAT_LOGIN_TOKEN_ERROR = 1007,
    CHAT_LOGIN_UID_ERROR = 1008,
    MYSQL_ERROR = 1100,
};

enum class MessageID : uint16_t {
    ID_GET_VERIFY_CODE = 1001,  // 获取验证码
    ID_REG_USER = 1002,         // 注册用户
    ID_RESET_PWD = 1003,        // 重置密码
    ID_USER_LOGIN = 1004,       // 登录用户
    ID_CHAT_LOGIN = 1005,       // 登录聊天服务器
    ID_CHAT_LOGIN_RSP = 1006,   // 登录聊天服务器响应
    ID_USER_SEARCH_REQ = 1007,  // 用户搜索请求
    ID_USER_SEARCH_RSP = 1008,  // 用户搜索响应
    ID_ADD_FRIEND_REQ = 1009,   // 添加用户请求
    ID_ADD_FRIEND_RSP = 1010,   // 添加用户响应
    ID_NOTIFY_FRIEND_ADD = 1011,   // 通知用户好友申请
    ID_FRIEND_AUTH_REQ = 1012,   // 同意添加好友请求
    ID_FRIEND_AUTH_RSP = 1013,   // 同意添加好友响应
    ID_NOTIFY_FRIEND_AUTH = 1014,   // 通知用户好友认证
    INVALID_ID,
};

struct UserInfo {
    int uid = 0;
    std::string name;
    std::string email;
    std::string password;
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