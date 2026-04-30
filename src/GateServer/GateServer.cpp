//
// Created by Fan on 2026/4/30.
//

#include <exception>

#include "GateServer.h"
#include "HttpConnection.h"

GateServer::GateServer(net::io_context &ioContext, const unsigned short &port)
    : acceptor_(ioContext, tcp::endpoint(tcp::v4(), port))
    , ioContext_(ioContext)
    , socket_(ioContext, port) {
}

void GateServer::start() {
    auto self = shared_from_this();
    acceptor_.async_accept(socket_, [self](boost::system::error_code ec) {
        try {
            // 错误处理放弃连接，继续监听其他连接
            if (ec) {
                self->start();
                return;
            }

            // 管理连接的读写
            std::make_shared<HttpConnection>(std::move(self->socket_))->start();
            // 继续监听
            self->start();
        } catch (std::exception) {
        }
    });
}
