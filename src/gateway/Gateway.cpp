#include "Gateway.h"

#include <iostream>
#include <thread>
#include <chrono>

namespace gateway {

Gateway::Gateway(boost::asio::io_context& io_context)
    : io_context_(io_context),
      is_running_(false) {
    initializeConnectionManager();
    initializeAuthCenter();
}

Gateway::~Gateway() {
    stop();
}

void Gateway::initialize(const GatewayConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    config_ = config;

    connection_manager_->setMaxConnections(config.max_connections);
    connection_manager_->setIdleTimeout(std::chrono::seconds(config.idle_timeout));
    connection_manager_->setEnableStatistics(true);

    auth_center_->initialize(config.auth_config);

    initializeServers();
}

void Gateway::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (is_running_) {
        return;
    }
    
    if (tcp_server_) {
        tcp_server_->setMessageHandler([this](network::ConnectionId connection_id, const std::vector<char>& data) {
            this->handleMessage(connection_id, data);
        });
        tcp_server_->setStateChangeHandler([this](network::ConnectionId connection_id, network::ConnectionState old_state, network::ConnectionState new_state) {
            this->handleStateChange(connection_id, old_state, new_state);
        });
        tcp_server_->setCloseHandler([this](network::ConnectionId connection_id, const boost::system::error_code& ec) {
            this->handleClose(connection_id, ec);
        });
        tcp_server_->start();
    }
    if (websocket_server_) {
        websocket_server_->setMessageHandler([this](network::ConnectionId connection_id, const std::vector<char>& data) {
            this->handleMessage(connection_id, data);
        });
        websocket_server_->setStateChangeHandler([this](network::ConnectionId connection_id, network::ConnectionState old_state, network::ConnectionState new_state) {
            this->handleStateChange(connection_id, old_state, new_state);
        });
        websocket_server_->setCloseHandler([this](network::ConnectionId connection_id, const boost::system::error_code& ec) {
            this->handleClose(connection_id, ec);
        });
        websocket_server_->start();
    }
    if (http_server_) {
        http_server_->setMessageHandler([this](network::ConnectionId connection_id, const std::vector<char>& data) {
            this->handleMessage(connection_id, data);
        });
        http_server_->setStateChangeHandler([this](network::ConnectionId connection_id, network::ConnectionState old_state, network::ConnectionState new_state) {
            this->handleStateChange(connection_id, old_state, new_state);
        });
        http_server_->setCloseHandler([this](network::ConnectionId connection_id, const boost::system::error_code& ec) {
            this->handleClose(connection_id, ec);
        });
        http_server_->start();
    }
    
    is_running_ = true;
    std::cout << "Gateway started successfully!" << std::endl;
    std::cout << "TCP server listening on port " << config_.tcp_port << std::endl;
    std::cout << "WebSocket server listening on port " << config_.websocket_port << std::endl;
    std::cout << "HTTP server listening on port " << config_.http_port << std::endl;
}

void Gateway::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!is_running_) {
        return;
    }
    
    // 停止所有服务器
    if (tcp_server_) {
        tcp_server_->stop();
    }
    
    if (websocket_server_) {
        websocket_server_->stop();
    }
    
    if (http_server_) {
        http_server_->stop();
    }
    
    // 关闭所有连接
    connection_manager_->closeAllConnections();
    
    is_running_ = false;
    std::cout << "Gateway stopped!" << std::endl;
}

void Gateway::sendMessage(network::ConnectionId connection_id, const std::shared_ptr<protocol::Message>& message) {
    auto connection = connection_manager_->getConnection(connection_id);
    if (connection) {
        auto data = message->serialize();
        connection->send(data);
    }
}

void Gateway::sendMessage(network::ConnectionId connection_id, const std::vector<char>& data) {
    auto connection = connection_manager_->getConnection(connection_id);
    if (connection) {
        connection->send(data);
    }
}

const GatewayConfig& Gateway::getConfig() const {
    return config_;
}

std::shared_ptr<network::ConnectionManager> Gateway::getConnectionManager() const {
    return connection_manager_;
}

network::ConnectionManager::GlobalStats Gateway::getGlobalStats() const {
    return connection_manager_->getGlobalStats();
}

void Gateway::handleMessage(network::ConnectionId connection_id, const std::vector<char>& data) {
    protocol_manager_->asyncProcessData(connection_id, data, [this, connection_id](const boost::system::error_code& ec) {
        if (!ec) {
            // 消息处理成功
        } else {
            std::cout << "Error processing message for connection " << connection_id << ": " << ec.message() << std::endl;
        }
    });
}

void Gateway::handleStateChange(network::ConnectionId connection_id, network::ConnectionState old_state, network::ConnectionState new_state) {
    // 可以添加状态变更处理逻辑
    if (config_.enable_debug_log) {
        if (connection_id) {
            std::cout << "Connection state changed: " << connection_id << " "
                      << network::connectionStateToString(old_state) << " -> " 
                      << network::connectionStateToString(new_state) << std::endl;
        }
    }
}

void Gateway::handleClose(network::ConnectionId connection_id, const boost::system::error_code &ec) {
    if (config_.enable_debug_log) {
        if (ec) {
            std::cout << "Connection " << connection_id << " closed with error: " << ec.message() << std::endl;
        } else {
            std::cout << "Connection " << connection_id << " closed successfully" << std::endl;
        }
    }
}

void Gateway::initializeServers() {
    tcp_server_ = std::make_unique<network::TcpServer>(io_context_, *connection_manager_, "0.0.0.0", config_.tcp_port);
    websocket_server_ = std::make_unique<network::WebSocketServer>(io_context_, *connection_manager_, "0.0.0.0", config_.websocket_port);
    http_server_ = std::make_unique<network::HttpServer>(io_context_, *connection_manager_, "0.0.0.0", config_.http_port);
}

void Gateway::initializeConnectionManager() {
    connection_manager_ = std::make_shared<network::ConnectionManager>();
    connection_manager_->setConnectionEventHandler([this](network::ConnectionId connection_id, network::ConnectionEvent event) {
        std::cout << "[Gateway]Connection event: " << connection_id << " - " << network::connectionEventToString(event) << std::endl;
    });
}

void Gateway::initializeProtocolManager() {
    protocol_manager_ = std::make_shared<protocol::ProtocolManager>(*connection_manager_);
}

void Gateway::initializeAuthCenter() {
    auth_center_ = std::make_shared<AuthCenter>();
}

} // namespace gateway