//
// Created by fan on 24-12-7.
//

#ifndef IMSERVER_BASESERVICE_H
#define IMSERVER_BASESERVICE_H

#include "Poco/Mutex.h"
#include "Poco/Net/SocketReactor.h"
#include "ByteStream.h"
#include "zmq.hpp"
#include "ZMQSocketWrapper.h"

namespace Base {

 /**
 * @class BaseService
 * @brief 基础服务发布，负责发布服务，客户端请求响应。
 * @note 服务发布消息采用json格式：
 *  {
 *     "type": "service_info",
 *     "name": "AccountService",
 *     "staus": "active",
 *     "timestamp": "%Y-%m-%d %H:%M:%S",
 *     "request_port": "1234",
 *     "response_port": "1235",
 *     "status_updates_port": "1236"
 *  }
 */
class BaseService {
public:
    BaseService(const char* serviceName, const char* servicePort, const char* requestPort, const char* responsePort);
    ~BaseService();

    // 服务初始化
    void initialize();

    // 服务启动
    void start();

    // 消息处理
    void handleAccountMessage(const std::string& message);
    void publishAccountUpdate(const std::string& userId, const std::string& updateType);

private:
    void setupSockets();
    void setupReactor();

    void onPublish(Poco::Net::ReadableNotification *pNotification);
    void onReadable(Poco::Net::ReadableNotification *pNotification);
    void onWritable(Poco::Net::ReadableNotification *pNotification);

    void publishServiceInfo();

private:
    static constexpr int SOCKET_BUFFER_LEN = 1024;

    zmq::context_t context;
    Poco::Net::SocketReactor reactor;

    std::string serviceName;
    std::string publishPort;
    std::string requestPort;
    std::string responsePort;

    std::unique_ptr<zmq::socket_t> publisher;         // 用于发布服务信息和账户状态更新
    std::unique_ptr<zmq::socket_t> puller;            // 用于接收客户端请求
    std::unique_ptr<zmq::socket_t> pusher;            // 用于发送响应

    Base::ByteStream publishMsgBuf;
    Base::ByteStream recvMsgBuf;
    Base::ByteStream sendMsgBuf;
};

}




#endif //IMSERVER_BASESERVICE_H
