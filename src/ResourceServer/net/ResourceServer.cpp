//
// Created by Fan on 2026/7/2.
//

#include "ResourceServer.h"

#include <iostream>

#include "HttpConnection.h"
#include "AsioIOServicePool.h"

ResourceServer::ResourceServer(net::io_context& ioContext, const unsigned short port)
    : ioContext_(ioContext)
    , acceptor_(ioContext, tcp::endpoint(tcp::v4(), port)) {
}

void ResourceServer::start() {
    std::cout << "ResourceServer HTTP service listening on port: "
              << acceptor_.local_endpoint().port() << std::endl;
    acceptLoop();
}

void ResourceServer::acceptLoop() {
    auto self = shared_from_this();
    auto& io_context = AsioIOServicePool::getInstance()->getIOService();
    auto connection = std::make_shared<HttpConnection>(io_context);
    acceptor_.async_accept(connection->getSocket(),
        [self, connection](const boost::system::error_code& ec) {
            try {
                if (ec) {
                    std::cerr << "HTTP accept error: " << ec.message() << std::endl;
                    self->acceptLoop();
                    return;
                }
                connection->start();
                self->acceptLoop();
            } catch (const std::exception& e) {
                std::cerr << "Accept exception: " << e.what() << std::endl;
            }
        });
}
