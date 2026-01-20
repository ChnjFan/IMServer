#include "Message.h"

// 包含消息子类头文件
#include "TcpMessage.h"
#include "WebSocketMessage.h"
#include "HttpMessage.h"
#include "IdGenerator.h"

namespace protocol {

// ---------------------- Message基类实现 ----------------------

Message::Message() {
    connection_id_ = 0;
    connection_type_ = network::ConnectionType::TCP;
    message_id_ = std::to_string(imserver::tool::IdGenerator::getInstance().generateMessageId());
}

Message::Message(const std::vector<char>& body,
                network::ConnectionId connection_id,
                network::ConnectionType connection_type) {
    body_ = body;
    connection_id_ = connection_id;
    connection_type_ = connection_type;
    message_id_ = std::to_string(imserver::tool::IdGenerator::getInstance().generateMessageId());
}

/**
 * @brief 将消息类型转换为字符串
 * @param type 消息类型
 * @return std::string 消息类型的字符串表示
 */
std::string Message::messageConnectionTypeToString(network::ConnectionType type)
{
    switch (type) {
        case network::ConnectionType::TCP:
            return "TCP";
        case network::ConnectionType::HTTP:
            return "HTTP";
        case network::ConnectionType::WebSocket:
            return "WebSocket";
        default:
            return "Unknown";
    }
}

// ---------------------- MessageSerializer工具类实现 ----------------------

std::vector<char> MessageSerializer::serialize(const Message& message) {
    return message.serialize();
}

std::shared_ptr<Message> MessageSerializer::deserialize(network::ConnectionType type, const std::vector<char> &data)
{
    std::shared_ptr<Message> message;

    switch (type) {
        case network::ConnectionType::HTTP:
            message = std::make_shared<HttpMessage>();
            break;
        case network::ConnectionType::TCP:
            message = std::make_shared<TCPMessage>();
            break;
        case network::ConnectionType::WebSocket:
            message = std::make_shared<WebSocketMessage>();
            break;
        default:
            return nullptr;
    }
    
    if (message && message->deserialize(data)) {
        return message;
    }
    
    return nullptr;
}

} // namespace protocol
