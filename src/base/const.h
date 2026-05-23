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
    CHAT_LOGIN_TOKEN_ERROR = 1007,
    CHAT_LOGIN_UID_ERROR = 1008,
    MYSQL_ERROR = 1100,
    REDIS_ERROR = 1101,
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
    ID_CHAT_MSG_REQ = 1015,         // 聊天消息请求
    ID_CHAT_MSG_RSP = 1016,         // 聊天消息响应
    ID_NOTIFY_CHAT_MSG = 1017,      //  推送聊天消息
    ID_CHAT_CONVERSATION_REQ = 1018,    // 会话创建请求
    ID_CHAT_CONVERSATION_RSP = 1019,    // 会话创建响应
    INVALID_ID,
};

enum class ChatMsgType : uint8_t {
    TEXT = 1,       // 文本消息
};

enum class ChatMsgStatus : uint8_t {
    SENDING = 0,       // 发送中
    IS_SEND = 1,       // 已发送
    IS_READ = 2,       // 已读
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

inline long long get_current_ms()
{
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto ms = duration_cast<milliseconds>(now.time_since_epoch());
    return ms.count();
}

inline std::string ms_to_datetime(const long long timestamp_ms)
{
    // 1. 毫秒 → 秒
    const time_t sec = timestamp_ms / 1000;

    // 2. 转为本地时间
    tm time_info{};
    localtime_r(&sec, &time_info); // 线程安全！！

    // 3. 格式化成标准字符串
    char buf[32];
    snprintf(buf, sizeof(buf),
             "%04d-%02d-%02d %02d:%02d:%02d",
             time_info.tm_year + 1900,
             time_info.tm_mon + 1,
             time_info.tm_mday,
             time_info.tm_hour,
             time_info.tm_min,
             time_info.tm_sec);

    return std::string(buf);
}

#endif //IMSERVER_CONST_H