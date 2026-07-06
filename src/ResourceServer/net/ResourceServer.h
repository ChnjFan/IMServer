//
// Created by Fan on 2026/7/2.
//

#ifndef IMSERVER_RESOURCEHTTPSERVICE_H
#define IMSERVER_RESOURCEHTTPSERVICE_H

#include <memory>

#include "const.h"

class ResourceServer : public std::enable_shared_from_this<ResourceServer> {
public:
    ResourceServer(net::io_context& ioContext, unsigned short port);
    void start();
private:
    void acceptLoop();

    net::io_context& ioContext_;
    tcp::acceptor acceptor_;
};


#endif //IMSERVER_RESOURCEHTTPSERVICE_H
