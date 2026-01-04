#include "Message.h"

// 包含消息子类头文件
#include "TcpMessage.h"
#include "WebSocketMessage.h"
#include "HttpMessage.h"

namespace protocol {

// ---------------------- Message基类实现 ----------------------

Message::Message() {
    connection_id_ = 0;
    connection_type_ = network::ConnectionType::TCP;
}

Message::Message(const std::vector<char>& body,
                network::ConnectionId connection_id,
                network::ConnectionType connection_type) {
    body_ = body;
    connection_id_ = connection_id;
    connection_type_ = connection_type;
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
