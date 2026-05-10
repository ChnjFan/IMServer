//
// Created by Fan on 2026/5/10.
//

#include "StatusServer.h"

#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <grpcpp/grpcpp.h>

#include "ConfigMgr.h"
#include "StatusServiceImpl.h"

StatusServer::StatusServer() {
}

void StatusServer::start() {
    auto& config = ConfigMgr::getInstance();

    const std::string serverAddress = config["StatusServer"]["Host"] + ":" + config["StatusServer"]["Port"];

    grpc::ServerBuilder builder;
    StatusServiceImpl service;
    builder.AddListeningPort(serverAddress, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << serverAddress << std::endl;

    // 捕获 CtrlC
    boost::asio::io_context io_context;
    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    // 异步等待信号
    signals.async_wait([&server, &io_context](const boost::system::error_code& error, int signal_number ) {
        if (!error) {
            std::cout << "Stopping server..." << std::endl;
            server->Shutdown();
            io_context.stop();
        }
    });
    // 其他线程执行 io_context
    std::thread([&io_context]() { io_context.run(); }).detach();
    server->Wait();
}
