//
// Created by fan on 24-12-7.
//

#ifndef IMSERVER_BASESERVICE_H
#define IMSERVER_BASESERVICE_H

#include <vector>
#include "Poco/Mutex.h"
#include "Poco/ThreadPool.h"
#include "Poco/Net/SocketReactor.h"
#include "ByteStream.h"
#include "BlockingQueue.h"
#include "zmq.hpp"
#include "ZMQSocketWrapper.h"
#include "ServiceParam.h"

namespace Base {

class ZMQMessage {
public:
    ZMQMessage(std::vector<zmq::message_t>&& parts, ByteStream& message) : routingParts(std::move(parts)), content(message) { }
    ZMQMessage() = default;
    ZMQMessage(ZMQMessage&& other) noexcept
            : routingParts(std::move(other.routingParts))
            , content(std::move(other.content)) { }

    std::vector<zmq::message_t>& getRoutingParts() {
        return routingParts;
    }

    ByteStream& getContent() {
        return content;
    };

private:
    std::vector<zmq::message_t> routingParts;  // 路由信息
    ByteStream content;
};

 /**
 * @class BaseService
 * @brief 基础服务发布，负责发布服务，客户端请求响应。
 * @note 服务发布消息采用json格式：
 *  {
 *     "type": "service_info",
 *     "name": "AccountService",
 *     "staus": "active",
 *     "timestamp": "%Y-%m-%d %H:%M:%S",
 *     "route_port": "1234",
 *     "status_updates_port": "1235"
 *  }
 */
class BaseService : public Poco::Runnable {
public:
    explicit BaseService(ServiceParam& param);
    ~BaseService();

    // 服务初始化
    void initialize();

    // 服务启动
    void start();

    // 消息处理
    void run() override;

    // 消息回复
    void sendResponse(const std::vector<zmq::message_t>& routingParts, ByteStream& response);
    void sendResponse(const std::vector<zmq::message_t>& routingParts, char* response, size_t size);

protected:
    virtual void messageProcessor();
    BlockingQueue<ZMQMessage>& getMessageQueue();

private:
    void setupSockets();
    void setupReactor();
    void setupThreadPool();

    void onReadable(Poco::Net::ReadableNotification *pNotification);

    void publishServiceInfo();

private:
    static constexpr int SOCKET_BUFFER_LEN = 1024;

    zmq::context_t context;
    Poco::Net::SocketReactor reactor;

    uint32_t threadPoolSize;
    Poco::ThreadPool threadPool;

    std::string serviceName;
    std::string publishPort;
    std::string routePort;

    zmq::socket_t publisher;         // 用于发布服务信息和账户状态更新
    zmq::socket_t router;            // 用于收发客户端请求

    BlockingQueue<ZMQMessage> messageQueue;
};

}




#endif //IMSERVER_BASESERVICE_H
