//
// Created by Fan on 2026/4/30.
//

#ifndef IMSERVER_CONST_H
#define IMSERVER_CONST_H

#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/asio.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

constexpr int DEFAULT_RPC_POOL_SIZE = 10;
constexpr int DEFAULT_REDIS_POOL_SIZE = 5;

#define CODE_PREFIX "code_"

enum class ErrorCodes : int32_t {
    SUCCESS = 0,
    ERROR_JSON = 1001,
    RPC_FAILED = 1002,
    VERIFY_CODE_EXPIRED = 1003,
    VERIFY_CODE_NOT_REACHED = 1004,
};

#endif //IMSERVER_CONST_H