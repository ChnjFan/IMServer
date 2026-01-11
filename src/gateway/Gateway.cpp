#include "Gateway.h"
#include "AuthCenter.h"

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

    auth_center_->initialize(config);

    initializeServers();
}

void Gateway::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (is_running_) {
        return;
    }
    
    if (tcp_server_) {
        tcp_server_->setMessageHandler([this](network::Connection::Ptr connection, const std::vector<char>& data) {
            this->handleMessage(connection, data);
        });
        tcp_server_->setStateChangeHandler([this](network::Connection::Ptr connection, network::ConnectionState state) {
            this->handleStateChange(connection, state);
        });
        tcp_server_->setCloseHandler([this](network::Connection::Ptr connection) {
            this->handleClose(connection);
        });
        tcp_server_->start();
    }
    if (websocket_server_) {
        websocket_server_->setMessageHandler([this](network::Connection::Ptr connection, const std::vector<char>& data) {
            this->handleMessage(connection, data);
        });
        websocket_server_->setStateChangeHandler([this](network::Connection::Ptr connection, network::ConnectionState state) {
            this->handleStateChange(connection, state);
        });
        websocket_server_->setCloseHandler([this](network::Connection::Ptr connection) {
            this->handleClose(connection);
        });
        websocket_server_->start();
    }
    if (http_server_) {
        http_server_->setMessageHandler([this](network::Connection::Ptr connection, const std::vector<char>& data) {
            this->handleMessage(connection, data);
        });
        http_server_->setStateChangeHandler([this](network::Connection::Ptr connection, network::ConnectionState state) {
            this->handleStateChange(connection, state);
        });
        http_server_->setCloseHandler([this](network::Connection::Ptr connection) {
            this->handleClose(connection);
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

size_t Gateway::handleMessage(network::Connection::Ptr connection, const std::vector<char>& data) {
    protocol_manager_->doProcessData(connection, data, [this, connection](const boost::system::error_code& ec) {
        if (!ec) {
            // 消息处理成功
        } else {
            std::cout << "Error processing message for connection " << connection->getId() << ": " << ec.message() << std::endl;
        }
    });
    
    return data.size();
}

void Gateway::handleStateChange(network::Connection::Ptr connection, network::ConnectionState old_state, network::ConnectionState new_state) {
    // 可以添加状态变更处理逻辑
    if (config_.enable_debug_log) {
        if (connection) {
            std::cout << "Connection state changed: " << connection->getRemoteAddress() << ":" 
                      << connection->getRemotePort() << " (ID: " << connection->getId() << ") "
                      << network::connectionStateToString(old_state) << " -> " 
                      << network::connectionStateToString(new_state) << std::endl;
        }
    }
}

void Gateway::handleClose(network::Connection::Ptr connection, const boost::system::error_code &ec) {
    if (config_.enable_debug_log) {
        if (ec) {
            std::cout << "Connection " << connection->getId() << " closed with error: " << ec.message() << std::endl;
        } else {
            std::cout << "Connection " << connection->getId() << " closed successfully" << std::endl;
        }
    }
}

void Gateway::initializeServers() {
    tcp_server_ = std::make_unique<network::TcpServer>(io_context_, connection_manager_, "0.0.0.0", config_.tcp_port);
    websocket_server_ = std::make_unique<network::WebSocketServer>(io_context_, connection_manager_, "0.0.0.0", config_.websocket_port);
    http_server_ = std::make_unique<network::HttpServer>(io_context_, connection_manager_, "0.0.0.0", config_.http_port);
}

void Gateway::initializeConnectionManager() {
    connection_manager_ = std::make_shared<network::ConnectionManager>();
    connection_manager_->setConnectionEventHandler([this](network::ConnectionId connection_id, network::ConnectionEvent event) {
        std::cout << "[Gateway]Connection event: " << connection_id << " - " << network::connectionEventToString(event) << std::endl;
    });
}

void Gateway::initializeProtocolManager() {
    protocol_manager_ = std::make_shared<ProtocolManager>(*connection_manager_);
}

void Gateway::initializeAuthCenter() {
    auth_center_ = std::make_shared<AuthCenter>();
}

} // namespace gateway