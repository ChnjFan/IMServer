//
// Created by fan on 24-12-6.
//

#ifndef IMSERVER_ACCOUNTSERVICE_H
#define IMSERVER_ACCOUNTSERVICE_H

#include "Poco/Mutex.h"
#include "Poco/Net/SocketReactor.h"
#include "IM.AccountServer.pb.h"
#include "zmq.hpp"

/**
 * @class AccountService
 * @brief 用户相关服务
 */
class AccountService {
public:
    AccountService(Poco::Net::SocketReactor& reactor);
    ~AccountService();

    /**
     * @brief 服务初始化
     */
    void initialize();




private:
    static Poco::Mutex mutex;
    zmq::context_t context;
    /**
     * @brief 服务发布广播socket
     */
    zmq::socket_t publisher;
    /**
     * @brief 消息推送socket
     */
    zmq::socket_t pusher;
    /**
     * @brief 消息拉取socket
     */
    zmq::socket_t poller;
};


#endif //IMSERVER_ACCOUNTSERVICE_H
