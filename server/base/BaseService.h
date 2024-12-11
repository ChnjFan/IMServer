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
    ZMQMessage() = default;

    ZMQMessage(ZMQMessage &message) {
        identity.copy(message.identity);
    }


    ZMQMessage(ZMQMessage const &message) {
        identity.rebuild(message.identity.data(), message.identity.size());
        msg.rebuild(message.msg.data(), message.msg.size());
    }

    ZMQMessage& operator=(const ZMQMessage& message) {
        identity.rebuild(message.identity.data(), message.identity.size());
        msg.rebuild(message.msg.data(), message.msg.size());
        return *this;
    }

    zmq::message_t &getIdentity() const {
        return (zmq::message_t &) std::move(identity);
    }

    void setIdentity(zmq::message_t &pIdentity) {
        ZMQMessage::identity.copy(pIdentity);
    }

    zmq::message_t &getMsg() const {
        return (zmq::message_t &) std::move(msg);
    }

    void setMsg(zmq::message_t &pMsg) {
        ZMQMessage::msg.copy(pMsg);
    }


private:
    zmq::message_t identity;
    zmq::message_t msg;
};

class BaseWorker : public Poco::Runnable {
public:
    BaseWorker(zmq::context_t &ctx, zmq::socket_type socType) : ctx(ctx), worker(ctx, socType) { }

    void run() override;

    virtual void onReadable();
    virtual void onError();

private:
    zmq::context_t &ctx;
    zmq::socket_t worker;

    BlockingQueue<ZMQMessage> recvMsgQueue;
    BlockingQueue<ZMQMessage> sendMsgQueue;
};

 /**
 * @class BaseService
 * @brief 基础服务发布，负责发布服务，客户端请求响应。
 * @note 服务发布消息采用json格式：
 *  {
 *     "type": "service_info",
 *     "name": "AccountService",
 *     "status": "active",
 *     "timestamp": "%Y-%m-%d %H:%M:%S",
 *     "route_port"staus: "1234",
 *     "status_updates_port": "1235"
 *  }
 */
class BaseService {
public:
    explicit BaseService(ServiceParam& param);
    ~BaseService();

    // 服务启动
    void start(Base::BaseWorker &worker);

private:
    // 服务初始化
    void initialize();

    void setupSockets();

    void startWorkThread(Base::BaseWorker &worker);

    void publishServiceInfo();
protected:
    zmq::context_t context;

private:
    static constexpr int SOCKET_BUFFER_LEN = 1024;


    uint32_t threadPoolSize;
    Poco::ThreadPool threadPool;

    std::string serviceName;
    std::string publishPort;
    std::string routePort;

    zmq::socket_t publisher;         // 用于发布服务信息和账户状态更新
    zmq::socket_t frontend;
    zmq::socket_t backend;

};

}




#endif //IMSERVER_BASESERVICE_H
