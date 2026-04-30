//
// Created by Fan on 2026/4/30.
//

#ifndef IMSERVER_GATESERVER_H
#define IMSERVER_GATESERVER_H

#include <memory>

#include "const.h"

class GateServer : public std::enable_shared_from_this<GateServer> {
public:
    GateServer(net::io_context& ioContext, const unsigned short& port);
    void start();
private:
    tcp::acceptor acceptor_;
    net::io_context& ioContext_;
    tcp::socket socket_;
};


#endif //IMSERVER_GATESERVER_H