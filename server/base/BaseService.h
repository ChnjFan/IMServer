//
// Created by fan on 24-12-7.
//

#ifndef IMSERVER_BASESERVICE_H
#define IMSERVER_BASESERVICE_H

#include <vector>
#include "zmq.hpp"
#include "Poco/Mutex.h"
#include "Poco/ThreadPool.h"
#include "Poco/Net/SocketReactor.h"
#include "Poco/Util/TimerTask.h"
#include "Poco/Util/Timer.h"
#include "ByteStream.h"
#include "BlockingQueue.h"
#include "ServiceParam.h"
#include "ZMQMessage.h"

namespace Base {

class BaseService;

class BaseWorker : public Poco::Runnable {
public:
    BaseWorker(zmq::context_t &ctx, zmq::socket_type socType) : ctx(ctx), worker(ctx, socType) { }

    void run() override;

    virtual void onReadable();
    virtual void onError();
    void send(Base::ZMQMessage& request, char* data, uint32_t size);
    void send(Base::ZMQMessage& request, google::protobuf::MessageLite& message, uint32_t size);

    BlockingQueue<ZMQMessage> &getRecvMsgQueue();
    BlockingQueue<ZMQMessage> &getSendMsgQueue();

private:
    zmq::context_t &ctx;
    zmq::socket_t worker;

    BlockingQueue<ZMQMessage> recvMsgQueue;
    BlockingQueue<ZMQMessage> sendMsgQueue;
};

class BaseServiceUpdate : public Poco::Util::TimerTask {
public:
    BaseServiceUpdate() = default;

    void setService(BaseService *pBaseService);
    void run() override;
    void updateServiceInfo();

private:
    BaseService *pService = nullptr;
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
    void start();

    void update(const std::string& content, zmq::send_flags flag);

    const std::string &getServiceName() const;
    std::string getRequestConnEndpoint() const;

protected:
    zmq::context_t context;

private:
    // 服务初始化
    void initialize();

    void setupSockets();

    void startMsgProxy();

private:
    Poco::ThreadPool threadPool;

    std::string serviceName;
    std::string publishEndpoint;
    std::string requestEndpoint;
    std::string requestConnEndpoint;

    zmq::socket_t publisher;         // 用于发布服务信息和账户状态更新
    zmq::socket_t frontend;
    zmq::socket_t backend;

    Poco::Util::Timer timer;
    BaseServiceUpdate serviceUpdate;
};

}




#endif //IMSERVER_BASESERVICE_H
