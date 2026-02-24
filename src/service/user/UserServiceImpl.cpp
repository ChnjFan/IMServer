#include "UserServiceImpl.h"
#include <iostream>
#include <sstream>

UserServiceImpl::UserServiceImpl() {
    // 初始化模拟用户数据
    userDatabase["admin"] = "password";
    userDatabase["user1"] = "123456";
    userDatabase["user2"] = "abc123";
    start_time_ = time(nullptr);
    std::cout << "UserService initialized with test users" << std::endl;
}

Status UserServiceImpl::Login(ServerContext* /*context*/, const LoginRequest* request, LoginResponse* response) {
    std::cout << "Received login request" << std::endl;
    
    const auto& base_message = request->base_message();
    
    // 设置响应的基础消息
    auto* response_base = response->mutable_base_message();
    response_base->set_message_id(base_message.message_id());
    response_base->set_source_service("service_user");
    response_base->set_target_service(base_message.source_service());
    response_base->set_timestamp(time(nullptr));
    
    try {
        // 从metadata中获取登录信息
        std::string username, password;
        bool hasUsername = false, hasPassword = false;
        
        // 遍历metadata获取用户名和密码
        for (const auto& pair : base_message.metadata()) {
            if (pair.first == "username") {
                username = pair.second;
                hasUsername = true;
            } else if (pair.first == "password") {
                password = pair.second;
                hasPassword = true;
            }
        }
        
        if (!hasUsername || !hasPassword) {
            response->set_error_code(im::common::protocol::ErrorCode::ERROR_CODE_INVALID_REQUEST);
            response->set_error_message("Username and password are required");
            std::cout << "Login failed: missing username or password" << std::endl;
            return Status::OK;
        }
        
        std::cout << "Attempting login for user: " << username << std::endl;
        
        // 验证用户
        if (validateUser(username, password)) {
            response->set_error_code(im::common::protocol::ErrorCode::ERROR_CODE_SUCCESS);
            response->set_error_message("Login successful");
            
            // 生成token并设置到响应
            std::string token = generateToken(username);
            std::string user_id = getUserID(username);
            
            (*response_base->mutable_metadata())["token"] = token;
            (*response_base->mutable_metadata())["user_id"] = user_id;
            (*response_base->mutable_metadata())["username"] = username;
            
            std::cout << "Login successful for user: " << username << std::endl;
        } else {
            response->set_error_code(im::common::protocol::ErrorCode::ERROR_CODE_INVALID_REQUEST);
            response->set_error_message("Invalid username or password");
            std::cout << "Login failed: invalid credentials for user: " << username << std::endl;
        }
    } catch (const std::exception& e) {
        response->set_error_code(im::common::protocol::ErrorCode::ERROR_CODE_INTERNAL_ERROR);
        response->set_error_message(std::string("Internal error: ") + e.what());
        std::cerr << "Login error: " << e.what() << std::endl;
    }
    
    return Status::OK;
}

Status UserServiceImpl::CheckStatus(ServerContext* /*context*/, const Empty* /*request*/, StatusResponse* response) {
    bool isHealthy = checkServiceHealth();
    
    response->set_is_healthy(isHealthy);
    response->set_queue_size(0); // 用户服务没有消息队列
    response->set_uptime_seconds(time(nullptr) - start_time_);
    
    std::cout << "Health check requested, status: " << (isHealthy ? "healthy" : "unhealthy") << std::endl;
    
    return Status::OK;
}

bool UserServiceImpl::validateUser(const std::string& username, const std::string& password) {
    auto it = userDatabase.find(username);
    return it != userDatabase.end() && it->second == password;
}

std::string UserServiceImpl::generateToken(const std::string& username) {
    std::stringstream tokenStream;
    tokenStream << "token_" << username << "_" << time(nullptr);
    return tokenStream.str();
}

std::string UserServiceImpl::getUserID(const std::string& username) {
    std::stringstream idStream;
    idStream << "user_" << username;
    return idStream.str();
}

bool UserServiceImpl::checkServiceHealth() {
    // 检查服务健康状态
    // 这里为了演示，直接返回健康
    // 实际项目中应该检查数据库连接、依赖服务等
    return true;
}
