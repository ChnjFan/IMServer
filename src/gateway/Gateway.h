#pragma once

#include <memory>
#include <vector>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <atomic>

#include <boost/asio.hpp>

#include "AuthCenter.h"
#include "network/ConnectionManager.h"
#include "network/TcpServer.h"
#include "network/WebSocketServer.h"
#include "network/HttpServer.h"
#include "protocol/Message.h"
#include "protocol/ProtocolManager.h"
#include "RoutingClient.h"

namespace gateway {

/**
 * @brief 网关配置参数
 */
struct GatewayConfig {
    // 监听端口配置
    uint16_t tcp_port = 8888;
    uint16_t websocket_port = 9999;
    uint16_t http_port = 8080;
    
    // 路由服务地址
    std::string routing_server_address = "localhost:50051";
    
    // 最大连接数
    size_t max_connections = 10000;
    
    // 空闲连接超时时间（秒）
    uint32_t idle_timeout = 300; // 5分钟
    
    // 认证配置
    AuthConfig auth_config;
    
    // 日志配置
    bool enable_debug_log = false;
};

/**
 * @brief 网关核心类
 * 
 * 负责管理所有协议服务器（TCP、WebSocket、HTTP），处理客户端连接和消息，
 * 进行认证和授权，以及消息路由和转发。
 */
class Gateway {
public:
    using MessageHandler = network::Connection::MessageHandler;
    using StateChangeHandler = network::Connection::StateChangeHandler;
    using CloseHandler = network::Connection::CloseHandler;

    Gateway(boost::asio::io_context& io_context);
    ~Gateway();
    
    Gateway(const Gateway&) = delete;
    Gateway& operator=(const Gateway&) = delete;
    
    void initialize(const GatewayConfig& config);
    void start();
    void stop();
    
    /**
     * @brief 发送消息给指定连接
     * @param connection_id 连接ID
     * @param message 消息对象
     */
    void sendMessage(network::ConnectionId connection_id, const std::shared_ptr<protocol::Message>& message);
    
    /**
     * @brief 发送消息给指定连接
     * @param connection_id 连接ID
     * @param data 消息数据
     */
    void sendMessage(network::ConnectionId connection_id, const std::vector<char>& data);
    
    /**
     * @brief 获取网关配置
     * @return const GatewayConfig& 网关配置
     */
    const GatewayConfig& getConfig() const;
    
    /**
     * @brief 获取连接管理器
     * @return std::shared_ptr<network::ConnectionManager> 连接管理器
     */
    std::shared_ptr<network::ConnectionManager> getConnectionManager() const;
    
    /**
     * @brief 获取全局统计信息
     * @return network::ConnectionManager::GlobalStats 全局统计信息
     */
    network::ConnectionManager::GlobalStats getGlobalStats() const;
    
private:
    // 处理消息
    void handleMessage(network::ConnectionId connection_id, const std::vector<char>& data);
    void handleStateChange(network::ConnectionId connection_id, network::ConnectionState old_state, network::ConnectionState new_state);
    void handleClose(network::ConnectionId connection_id, const boost::system::error_code& ec);
    
    // 初始化组件
    void initializeRoutingClient();
    void initializeServers();
    void initializeConnectionManager();
    void initializeProtocolManager();
    void initializeAuthCenter();

    void messageConverter(const protocol::Message &message, im::common::protocol::BaseMessage *pBaseMessage);

    // 核心组件
    boost::asio::io_context& io_context_;
    std::shared_ptr<network::ConnectionManager> connection_manager_;
    std::shared_ptr<protocol::ProtocolManager> protocol_manager_;
    std::shared_ptr<AuthCenter> auth_center_;
    std::unique_ptr<RoutingClient> routing_client_;
    
    // 各种协议服务器
    std::shared_ptr<network::TcpServer> tcp_server_;
    std::shared_ptr<network::WebSocketServer> websocket_server_;
    std::shared_ptr<network::HttpServer> http_server_;
    
    GatewayConfig config_;
    
    // 消息处理器
    MessageHandler message_handler_;
    StateChangeHandler state_change_handler_;
    CloseHandler close_handler_;
    
    // 状态
    std::atomic<bool> is_running_ = false;
    std::mutex mutex_;
};

} // namespace gateway