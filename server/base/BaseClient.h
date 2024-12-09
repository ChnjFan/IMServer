//
// Created by fan on 24-12-9.
//

#ifndef IMSERVER_BASECLIENT_H
#define IMSERVER_BASECLIENT_H

#include "Poco/Thread.h"
#include "zmq.hpp"

namespace Base {

class BaseClient : public Poco::Runnable {
public:
    BaseClient(std::string& serviceIP, uint32_t servicePort);
    ~BaseClient();

    void run() override;            // 监听服务状态

private:
    void initialize();

    void parseServiceUpdate(std::string& update);

private:
    std::string serviceIP;
    uint32_t servicePort;
    zmq::context_t context;

    zmq::socket_t subscriber;         // 用于订阅服务
    zmq::socket_t dealer;             // 处理请求应答
    uint32_t dealerPort;              // 服务端消息端口

    Poco::Thread listener;            // 服务状态监听线程
    std::atomic<bool> running{false};

    Poco::Mutex mutex;
};

}


#endif //IMSERVER_BASECLIENT_H
