//
// Created by Fan on 2026/4/30.
//

#ifndef IMSERVER_HTTPCONNECTION_H
#define IMSERVER_HTTPCONNECTION_H

#include <memory>
#include <string>
#include <unordered_map>

#include "const.h"

class HttpConnection : public std::enable_shared_from_this<HttpConnection> {
public:
    typedef std::unordered_map<std::string, std::string> UrlParams;

    friend class LogicSystem;

    explicit HttpConnection(boost::asio::io_context& io_context);
    void start();

    tcp::socket& getSocket() { return socket_; }

    class UrlParser {
    public:
        UrlParser() = default;
        void parse(const std::string& url);
        [[nodiscard]] const std::string& getPath() const;
        [[nodiscard]] const UrlParams& getParams() const;
        [[nodiscard]] bool hasParam(const std::string& key) const;
        [[nodiscard]] std::string getParam(const std::string& key, const std::string& defaultValue = "") const;
    private:
        std::string path_;
        UrlParams params_;

        static std::string urlDecode(const std::string& encoded);
    };

private:
    void checkDeadline();
    void writeResponse();
    void handleRequest();

    tcp::socket socket_;
    beast::flat_buffer buffer_{8192};
    http::request<http::dynamic_body> request_;
    http::response<http::dynamic_body> response_;
    net::steady_timer deadline_{socket_.get_executor(), std::chrono::seconds(60)};

    UrlParser urlParser_;
};


#endif //IMSERVER_HTTPCONNECTION_H