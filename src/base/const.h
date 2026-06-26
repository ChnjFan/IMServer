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
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <json/json.h>

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
    ERROR_REQUEST_JSON = 1001,
    RPC_FAILED = 1002,
    MYSQL_ERROR = 1003,
    REDIS_ERROR = 1004,
    FILE_ERROR = 1005,
    REQUEST_NOT_FOUND = 1006,

    VERIFY_CODE_EXPIRED = 2001,
    VERIFY_CODE_NOT_REACHED = 2002,

    USER_EXISTS = 3001,
    USER_EMAIL_NOT_EXISTS = 3002,
    USER_NOT_EXISTS = 3003,
    USER_IS_FRIEND_RELATION = 3004,
    USER_IS_OFFLINE = 3005,

    FRIEND_APPLY_NOT_EXISTS = 3101,
    FRIEND_NOT_EXISTS = 3102,

    CHAT_LOGIN_TOKEN_ERROR = 4001,
    CHAT_LOGIN_UID_ERROR = 4002,
};

enum class MessageID : uint16_t {
    ID_GET_VERIFY_CODE = 1001,  // 获取验证码
    ID_REG_USER = 1002,         // 注册用户
    ID_RESET_PWD = 1003,        // 重置密码
    ID_USER_LOGIN = 1004,       // 登录用户
    ID_CHAT_LOGIN = 1005,       // 登录聊天服务器
    ID_CHAT_LOGIN_RSP = 1006,   // 登录聊天服务器响应

    ID_FIRST_PAGE_REQ = 1101,   // 主页页面信息
    ID_FIRST_PAGE_RSP = 1102,   // 主页页面信息

    ID_USER_SEARCH_REQ = 2001,  // 用户搜索请求
    ID_USER_SEARCH_RSP = 2002,  // 用户搜索响应
    ID_FRIEND_APPLY_REQ = 2003,   // 添加用户请求
    ID_FRIEND_APPLY_RSP = 2004,   // 添加用户响应
    ID_NOTIFY_FRIEND_APPLY = 2005,   // 通知用户好友申请
    ID_FRIEND_AUTH_REQ = 2006,   // 同意添加好友请求
    ID_FRIEND_AUTH_RSP = 2007,   // 同意添加好友响应
    ID_NOTIFY_FRIEND_AUTH = 2008,   // 通知用户好友认证
    ID_GET_FRIEND_REPLY_REQ = 2009, // 获取好友申请请求
    ID_GET_FRIEND_REPLY_RSP = 2010, // 获取好友申请响应
    ID_GET_FRIEND_LIST_REQ = 2011,  // 获取好友列表请求
    ID_GET_FRIEND_LIST_RSP = 2012,  // 获取好友列表响应
    ID_UPDATE_FRIEND_REQ = 2013,    // 更新好友关系请求
    ID_UPDATE_FRIEND_RSP = 2014,    // 更新好友关系响应

    ID_UPDATE_USERINFO_REQ = 2101,  // 更新用户信息请求
    ID_UPDATE_USERINFO_RSP = 2102,  // 更新用户信息响应
    ID_GET_USER_FULL_INFO_REQ = 2103,   // 获取用户详细信息请求
    ID_GET_USER_FULL_INFO_RSP = 2104,   // 获取用户详细信息响应

    ID_CHAT_MSG_REQ = 3001,         // 聊天消息请求
    ID_CHAT_MSG_RSP = 3002,         // 聊天消息响应
    ID_NOTIFY_CHAT_MSG = 3003,      //  推送聊天消息
    ID_CHAT_UPLOAD_FILE_REQ = 3004,     // 上传文件请求
    ID_CHAT_UPLOAD_FILE_RSP = 3005,     // 上传文件响应
    ID_CHAT_DOWNLOAD_FILE_REQ = 3006,   // 下载文件请求
    ID_CHAT_DOWNLOAD_FILE_RSP = 3007,   // 下载文件响应

    ID_CHAT_CONVERSATION_REQ = 4001,    // 会话创建请求
    ID_CHAT_CONVERSATION_RSP = 4002,    // 会话创建响应
    ID_CONV_HISTORY_MSG_REQ = 4003,    // 会话历史消息请求
    ID_CONV_HISTORY_MSG_RSP = 4004,    // 会话历史消息请求

    ID_NOTIFY_OFFLINE = 5001,       // 通知客户端离线
    ID_HEART_BEAT_REQ = 5002,       // PING
    ID_HEART_BEAT_RSP = 5003,       // PONG
    ID_CLIENT_COMMON_RSP = 5004,    // 客户端通用应答
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
    char buf[72];
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

inline std::string base64_decode(const std::string& base64_str)
{
    using namespace boost::archive::iterators;
    // 去掉末尾填充符再解析
    std::string s = base64_str;
    while (!s.empty() && s.back() == '=')
        s.pop_back();

    // 转换链：base64字符 →6bit →8bit二进制
    using DecodeIter = transform_width<binary_from_base64<std::string::iterator>,8,6>;

    return {DecodeIter(s.begin()), DecodeIter(s.end())};
}

#endif //IMSERVER_CONST_H