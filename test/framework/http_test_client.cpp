#include "http_test_client.h"
#include <sstream>

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
    std::istringstream bodyStream(res.body());
    std::string errs;
    Json::parseFromStream(reader, bodyStream, &rsp.body, &errs);

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
