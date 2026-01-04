#include "Parser.h"

#include "TcpParser.h"
#include "WebSocketParser.h"
#include "HttpParser.h"

#include <unordered_map>

namespace protocol {

// -------------- ParserFactory 实现 --------------

ParserFactory::ParserFactory() {
    // 注册默认解析器
    registerParser(network::ConnectionType::TCP, []() {
        return std::make_shared<TcpParser>();
    });
    
    registerParser(network::ConnectionType::WebSocket, []() {
        return std::make_shared<WebSocketParser>();
    });
    
    registerParser(network::ConnectionType::HTTP, []() {
        return std::make_shared<HttpParser>();
    });
}

ParserFactory& ParserFactory::instance() {
    static ParserFactory instance;
    return instance;
}

std::shared_ptr<Parser> ParserFactory::createParser(network::ConnectionType connection_type) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 查找解析器创建函数
    auto it = parser_creators_.find(connection_type);
    if (it == parser_creators_.end()) {
        return nullptr;
    }
    
    // 尝试从缓存获取实例
    auto cache_it = parser_instances_.find(connection_type);
    if (cache_it != parser_instances_.end()) {
        auto parser = cache_it->second.lock();
        if (parser) {
            parser->reset();
            return parser;
        }
    }
    
    // 创建新实例
    auto parser = it->second();
    parser_instances_[connection_type] = parser;
    
    return parser;
}

void ParserFactory::registerParser(network::ConnectionType connection_type,
        std::function<std::shared_ptr<Parser>()> creator) {
    std::lock_guard<std::mutex> lock(mutex_);
    parser_creators_[connection_type] = std::move(creator);
}

} // namespace protocol
