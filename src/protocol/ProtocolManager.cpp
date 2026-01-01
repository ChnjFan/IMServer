#include "ProtocolManager.h"

#include <iostream>

namespace protocol {

ProtocolManager::ProtocolManager() : executor_(4, 4) { // 默认4个IO线程，4个CPU线程
    initializeParsers();
}

ProtocolManager& ProtocolManager::instance() {
    static ProtocolManager instance;
    return instance;
}

void ProtocolManager::registerHandler(
    uint16_t message_type,
    std::function<void(const Message&, network::Connection::Ptr)> handler) {
    message_router_.registerHandler(message_type, std::move(handler));
}

std::shared_ptr<Parser> ProtocolManager::getParser(network::ConnectionType connection_type) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = parsers_.find(connection_type);
    if (it != parsers_.end()) {
        return it->second;
    }
    
    // 如果没有找到，尝试从工厂创建
    auto parser = ParserFactory::instance().createParser(connection_type);
    if (parser) {
        parsers_[connection_type] = parser;
    }
    
    return parser;
}

void ProtocolManager::setThreadPoolSize(size_t io_threads, size_t cpu_threads) {
    executor_.setThreadPoolSize(AsyncExecutor::PoolType::IO, io_threads);
    executor_.setThreadPoolSize(AsyncExecutor::PoolType::CPU, cpu_threads);
}

void ProtocolManager::initializeParsers() {
    // 初始化各种协议解析器
    parsers_[network::ConnectionType::TCP] = std::make_shared<TcpParser>();
    parsers_[network::ConnectionType::WebSocket] = std::make_shared<WebSocketParser>();
    parsers_[network::ConnectionType::HTTP] = std::make_shared<HttpParser>();
}

void ProtocolManager::doProcessData(
    network::ConnectionId connection_id,
    std::vector<char> data,
    std::function<void(const boost::system::error_code&)> callback) {
    boost::system::error_code ec;
    
    try {
        // 获取连接对象
        auto connection = network::ConnectionManager::instance().getConnection(connection_id);
        if (!connection) {
            ec = boost::system::errc::make_error_code(boost::system::errc::bad_file_descriptor);
            callback(ec);
            return;
        }
        
        // 获取对应的解析器
        auto parser = getParser(connection->getType());
        if (!parser) {
            ec = boost::system::errc::make_error_code(boost::system::errc::not_supported);
            callback(ec);
            return;
        }
        
        // 设置连接ID到解析器
        parser->setConnectionId(connection_id);
        
        // 异步解析数据
        parser->asyncParse(data, [this, connection, callback](const boost::system::error_code& parse_ec, Message message) {
            if (parse_ec) {
                callback(parse_ec);
                return;
            }
            
            try {
                // 异步路由消息
                message_router_.asyncRoute(message, connection);
                callback(boost::system::error_code());
            } catch (const std::exception& e) {
                std::cerr << "Exception in message routing: " << e.what() << std::endl;
                callback(boost::system::errc::make_error_code(boost::system::errc::io_error));
            }
        });
    } catch (const std::exception& e) {
        std::cerr << "Exception in ProtocolManager::doProcessData: " << e.what() << std::endl;
        ec = boost::system::errc::make_error_code(boost::system::errc::io_error);
        callback(ec);
    }
}

} // namespace protocol
