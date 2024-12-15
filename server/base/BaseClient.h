//
// Created by fan on 24-12-9.
//

#ifndef IMSERVER_BASECLIENT_H
#define IMSERVER_BASECLIENT_H

#include "Poco/Thread.h"
#include "Poco/ThreadPool.h"
#include "zmq.hpp"

namespace Base {

class BaseClient : public Poco::Runnable {
public:
    BaseClient(std::string& serviceProxyEndPoint, Poco::ThreadPool& threadPool);
    ~BaseClient();

    void subscribe(std::string& serviceName);
    void send(std::string& content);

    void start();

    void run() override;            // 监听服务状态

private:
    void initialize();
    void parseServiceUpdate(std::string& update);
    void connectService();

private:
    std::string serviceProxyEndPoint;
    zmq::context_t context;

    /**
     * @brief 订阅服务
     */
    zmq::socket_t subscriber;
    zmq::socket_t clientSocket;
    /**
     * @brief 服务是否可用
     */
    std::atomic<bool> running{false};
    std::string serviceEndpoint;

    std::vector<std::string> subscribeService;

    Poco::ThreadPool& threadPool;
    Poco::Mutex mutex;
};

}


#endif //IMSERVER_BASECLIENT_H
