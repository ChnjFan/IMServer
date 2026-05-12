//
// Created by Fan on 2026/4/30.
//

#include <exception>

#include "GateServer.h"

#include <iostream>

#include "HttpConnection.h"
#include "AsioIOServicePool.h"

GateServer::GateServer(net::io_context &ioContext, const unsigned short &port)
    : acceptor_(ioContext, tcp::endpoint(tcp::v4(), port))
    , ioContext_(ioContext) {
}

void GateServer::start() {
    auto self = shared_from_this();
    auto & io_context = AsioIOServicePool::getInstance()->getIOService();
    auto connection = std::make_shared<HttpConnection>(io_context);
    acceptor_.async_accept(connection->getSocket(), [self, connection](const boost::system::error_code &ec) {
        try {
            // 错误处理放弃连接，继续监听其他连接
            if (ec) {
                self->start();
                return;
            }

            // 管理连接的读写
            connection->start();
            // 继续监听
            self->start();
        } catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
        }
    });
}
