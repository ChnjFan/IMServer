#ifndef IMSERVER_HTTP_TEST_CLIENT_H
#define IMSERVER_HTTP_TEST_CLIENT_H

#include <string>
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
