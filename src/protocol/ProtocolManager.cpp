#include "ProtocolManager.h"

#include <iostream>

namespace protocol {

ProtocolManager::ProtocolManager(network::ConnectionManager& connection_manager)
     : connection_manager_(connection_manager), executor_(4, 4) { // 默认4个IO线程，4个CPU线程
    initializeParsers();
}

void ProtocolManager::registerHandler(
    uint16_t message_type,
    std::function<void(const Message&, network::Connection::Ptr)> handler) {
    message_router_.registerHandler(message_type, std::move(handler));
}

std::shared_ptr<Parser> ProtocolManager::getParser(network::ConnectionId connection_id, network::ConnectionType connection_type) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 首先查找连接特定的解析器
    auto it = parsers_.find(connection_id);
    if (it != parsers_.end()) {
        return it->second;
    }
    
    // 如果没有找到，创建新的解析器实例
    auto parser = ParserFactory::instance().createParser(connection_type);
    if (parser) {
        // 为该连接存储解析器实例
        parsers_[connection_id] = parser;
    }
    
    return parser;
}

void ProtocolManager::setThreadPoolSize(size_t io_threads, size_t cpu_threads) {
    executor_.setThreadPoolSize(AsyncExecutor::PoolType::IO, io_threads);
    executor_.setThreadPoolSize(AsyncExecutor::PoolType::CPU, cpu_threads);
}

void ProtocolManager::initializeParsers() {
    // 初始化解析器工厂，不需要预创建解析器实例
    // 现在为每个连接创建独立的解析器实例
}

void ProtocolManager::doProcessData(network::ConnectionId connection_id, std::vector<char> data,
                                    std::function<void(const boost::system::error_code&)> callback) {
    boost::system::error_code ec;

    try {
        // 获取连接对象
        auto connection = connection_manager_.getConnection(connection_id);
        if (!connection) {
            ec = boost::system::errc::make_error_code(boost::system::errc::bad_file_descriptor);
            callback(ec);
            return;
        }

        // 获取对应的解析器
        auto parser = getParser(connection_id, connection->getType());
        if (!parser) {
            ec = boost::system::errc::make_error_code(boost::system::errc::not_supported);
            callback(ec);
            return;
        }
        
        // 设置连接ID到解析器
        parser->setConnectionId(connection_id);
        
        // 异步解析数据
        parser->asyncParse(data, [this, connection, callback](const boost::system::error_code& parse_ec, Message& message) {
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
