#include "Gateway.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <unordered_map>

#include "RoutingClient.h"
#include "JsonUtils.h"

namespace gateway {

Gateway::Gateway(boost::asio::io_context& io_context)
    : io_context_(io_context),
      is_running_(false) {
    initializeConnectionManager();
    initializeProtocolManager();
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

    initializeRoutingClient();
    initializeServers();
    std::cout << "Gateway initialized with config:" << std::endl;
    std::cout << "  Max Connections: " << config.max_connections << std::endl;
    std::cout << "  Idle Timeout: " << config.idle_timeout << " seconds" << std::endl;
}

void Gateway::start() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::cout << "Gateway starting..." << std::endl;
    
    if (is_running_) {
        return;
    }
    
    if (tcp_server_) {
        std::cout << "TCP server starting on port " << config_.tcp_port << std::endl;
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
        std::cout << "WebSocket server starting on port " << config_.websocket_port << std::endl;
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
        std::cout << "HTTP server starting on port " << config_.http_port << std::endl;
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
    if (config_.enable_debug_log) {
        std::cout << "Received message from connection " << connection_id << " with data: " << std::string(data.begin(), data.end()) << " bytes" << std::endl;
    }
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

void Gateway::initializeRoutingClient() {
    routing_client_ = std::make_unique<RoutingClient>(config_.routing_server_address);

    // 检查路由服务状态
    im::common::protocol::StatusResponse status;
    if (routing_client_->checkStatus(status)) {
        std::cout << "[Gateway] Routing service status: " 
                  << (status.is_healthy() ? "healthy" : "unhealthy") << std::endl;
        if (status.is_healthy()) {
            std::cout << "[Gateway] Queue size: " << status.queue_size() << std::endl;
            std::cout << "[Gateway] Uptime: " << status.uptime_seconds() << " seconds" << std::endl;
        }
    } else {
        std::cerr << "[Gateway] Failed to check routing service status" << std::endl;
    }
}

void Gateway::initializeServers() {
    tcp_server_ = std::make_shared<network::TcpServer>(io_context_, *connection_manager_, "0.0.0.0", config_.tcp_port);
    websocket_server_ = std::make_shared<network::WebSocketServer>(io_context_, *connection_manager_, "0.0.0.0", config_.websocket_port);
    http_server_ = std::make_shared<network::HttpServer>(io_context_, *connection_manager_, "0.0.0.0", config_.http_port);
}

void Gateway::initializeConnectionManager() {
    connection_manager_ = std::make_shared<network::ConnectionManager>();
    connection_manager_->setConnectionEventHandler([this](network::ConnectionId connection_id, network::ConnectionEvent event) {
        std::cout << "[Gateway]Connection event: " << connection_id << " - " << network::connectionEventToString(event) << std::endl;
    });
}

void Gateway::initializeProtocolManager() {
    protocol_manager_ = std::make_shared<protocol::ProtocolManager>(*connection_manager_);

    auto messageHandler = [this](const protocol::Message& message, network::Connection::Ptr connection) {
        try {
            std::cout << "[Gateway] Received message: " << connection->getConnectionId() << " - " << message.getMessageId() << std::endl;
            im::common::protocol::RouteRequest request;

            messageConverter(message, request.mutable_base_message());
            // 设置网关ID和优先级
            request.set_gateway_id("gateway_1"); // todo 实际应用中应该使用唯一的网关ID
            request.set_priority(5); // 默认优先级
            
            // 发送路由请求
            im::common::protocol::RouteResponse response;
            if (routing_client_->routeMessage(request, response)) {
                if (response.error_code() == im::common::protocol::ErrorCode::ERROR_CODE_SUCCESS) {
                    if (response.accepted()) {
                        std::cout << "[Gateway] Message routed successfully: " << message.getMessageId() << std::endl;
                    } else {
                        std::cerr << "[Gateway] Message rejected by routing service: " << message.getMessageId() << std::endl;
                    }
                } else {
                    std::cerr << "[Gateway] Routing failed with error: " << response.error_message() << std::endl;
                }
            } else {
                std::cerr << "[Gateway] Failed to send routing request: " << message.getMessageId() << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "[Gateway] Exception in message handler: " << e.what() << std::endl;
        }
    };
    
    // 注册各种连接类型的处理器
    protocol_manager_->registerHandler(network::ConnectionType::TCP, messageHandler);
    protocol_manager_->registerHandler(network::ConnectionType::WebSocket, messageHandler);
    protocol_manager_->registerHandler(network::ConnectionType::HTTP, messageHandler);
}

void Gateway::initializeAuthCenter() {
    auth_center_ = std::make_shared<AuthCenter>();
}

void Gateway::messageConverter(const protocol::Message &message, im::common::protocol::BaseMessage *pBaseMessage) {
    if (nullptr == pBaseMessage) {
        return;
    }

    // 消息ID格式包含连接类型和连接ID，用于区分不同连接类型的消息，
    // 例如：messid_TCP_1234567890
    pBaseMessage->set_message_id(message.getMessageId());
    pBaseMessage->set_source_service("gateway");
    pBaseMessage->set_target_service("routing");
    pBaseMessage->set_message_type(std::static_cast<int32_t>(message.getMessageType()));
    pBaseMessage->set_timestamp(imserver::tool::IdGenerator::getInstance().getCurrentTimestamp());
    
    std::unordered_map<std::string, std::string> metadata;
    if (imserver::tool::JsonUtils::jsonToMetadata(message.getPayload(), metadata)) {
        for (const auto& [key, value] : metadata) {
            (*pBaseMessage->mutable_metadata())[key] = value;
        }
    }
}

} // namespace gateway