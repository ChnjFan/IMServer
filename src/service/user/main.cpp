#include <iostream>
#include <string>
#include <grpcpp/grpcpp.h>

#include "UserServiceImpl.h"

void RunServer(int port) {
    std::string server_address = "0.0.0.0:" + std::to_string(port);
    UserServiceImpl service;

    ServerBuilder builder;
    // 配置服务器选项
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    // 设置最大poller数量
    builder.SetSyncServerOption(grpc::ServerBuilder::MAX_POLLERS, 10);
    
    // 构建并启动服务器
    std::unique_ptr<Server> server = builder.BuildAndStart();
    if (!server) {
        std::cerr << "Failed to start user service" << std::endl;
        return;
    }
    
    std::cout << "User service started successfully on " << server_address << std::endl;
    std::cout << "Test users available: admin/password, user1/123456, user2/abc123" << std::endl;
    
    // 阻塞等待服务器关闭
    server->Wait();
}

int main(int argc, char** argv) {
    // 默认端口
    int port = 50053;
    
    // 解析命令行参数
    if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
        } catch (const std::exception& e) {
            std::cerr << "Invalid port number: " << argv[1] << std::endl;
            std::cerr << "Using default port: 50053" << std::endl;
        }
    }
    
    std::cout << "Starting user service..." << std::endl;
    RunServer(port);
    
    return 0;
}
