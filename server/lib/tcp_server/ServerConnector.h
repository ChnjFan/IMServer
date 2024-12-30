//
// Created by fan on 24-12-29.
//

#ifndef IMSERVER_SERVERCONNECTOR_H
#define IMSERVER_SERVERCONNECTOR_H

#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/SocketReactor.h"
#include "BlockingQueue.h"
#include "Message.h"

namespace TcpServerNet {

/**
 * @class ServerConnector
 * @brief 服务连接类
 * @note 根据传入 socket 创建一个 acceptor 连接客户端
 */
template <class ServiceHandler>
class ServerConnector {
public:
    explicit ServerConnector(Poco::Net::StreamSocket& ss):
            _socket(ss),
            _pReactor(nullptr)
            { }

    ServerConnector(Poco::Net::StreamSocket& ss, Poco::Net::SocketReactor& reactor):
            _socket(ss),
            _pReactor(nullptr)
    {
        registerConnector(reactor);
        onConnect();
    }

    virtual ~ServerConnector()
    {
        unregisterConnector();
    }

private:
    void registerConnector(Poco::Net::SocketReactor& reactor);
    void onConnect();
    void unregisterConnector();

    Poco::Net::StreamSocket& _socket;
    Poco::Net::SocketReactor* _pReactor;
};

}

#endif //IMSERVER_SERVERCONNECTOR_H
