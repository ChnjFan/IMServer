#include <iostream>
#include <exception>

#include <grpcpp/grpcpp.h>

#include "ChatServer.h"
#include "ConfigMgr.h"
#include "AsioIOServicePool.h"
#include "ChatLogicSystem.h"
#include "ChatServiceImpl.h"
#include "RedisMgr.h"
#include "const.h"

int main()
{
    auto& config = ConfigMgr::getInstance();
    const auto portStr = config["ChatServer"]["Port"];
    if (portStr.empty()) {
        std::cout << "Chat server port config is empty!" << std::endl;
        return 0;
    }
    auto port = std::stoi(portStr);

    try {
        net::io_context io_context{1};

        std::make_shared<ChatServer>(io_context, port)->start();
        std::cout << "ChatServer started on port: " << port << std::endl;

        // 启动 grpc 服务
        ChatServiceImpl service;
        grpc::ServerBuilder builder;
        const std::string rpcAddress = config["ChatServer"]["Host"] + ":" + config["ChatServer"]["RPCPort"];
        builder.AddListeningPort(rpcAddress, grpc::InsecureServerCredentials());
        builder.RegisterService(&service);
        std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
        std::thread grpcServiceThread([&server] {
            server->Wait();
        });

        // 捕获信号结束进程
        auto pool = AsioIOServicePool::getInstance();
        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&io_context, &pool, &server](const boost::system::error_code &error, int signal_number) {
            if (error) {
                return;
            }
            boost::ignore_unused(signal_number);
            io_context.stop();
            pool->stop();
            server->Shutdown();
        });

        // ChatServer启动成功初始化 Redis 的连接计数
        const std::string serverName = config["ChatServer"]["Name"];
        ChatLogicSystem::getInstance()->setServerName(serverName);
        RedisMgr::getInstance()->hSet(LOGIN_COUNT, serverName, "0");
        io_context.run();

        RedisMgr::getInstance()->hDel(LOGIN_COUNT, serverName);
        RedisMgr::getInstance()->close();
        if (grpcServiceThread.joinable()) {
            grpcServiceThread.join();
        }
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return 0;
}
