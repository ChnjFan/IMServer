//
// Created by fan on 25-3-2.
//

#ifndef IMSERVER_IMSERVER_H
#define IMSERVER_IMSERVER_H

#include <string>
#include "Poco/ThreadPool.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/SocketReactor.h"
#include "Poco/Net/SocketAcceptor.h"
#include "Poco/Util/ServerApplication.h"
#include "Exception.h"
#include "MessageHandler.h"
#include "Message.h"
#include "BlockingQueue.h"

class ServerHandle {
public:
    static ServerHandle& getInstance() {
        static ServerHandle instance(port_);
        return instance;
    }

    void start();
    void setTask(Base::MessagePtr &message);
    Base::MessagePtr getTask();
    Base::MessagePtr getTask(long milliseconds);

    void sendResponse(Base::MessagePtr &message);
    Base::MessagePtr getResponse(long milliseconds);

    static unsigned short port_;
private:
    explicit ServerHandle(unsigned short port) : socket(port), reactor(), acceptor(socket, reactor), task(), response() { }

    ServerHandle(const ServerHandle&) = delete;
    ServerHandle& operator=(const ServerHandle&) = delete;

    Poco::Net::ServerSocket socket;
    Poco::Net::SocketReactor reactor;
    Poco::Net::SocketAcceptor<MessageHandler> acceptor;
    Base::BlockingQueue<Base::MessagePtr> task;
    Base::BlockingQueue<Base::MessagePtr> response;
};

/**
 * @brief 启动服务器
 * @param port
 */
void startServer(unsigned short port);

#endif //IMSERVER_IMSERVER_H
