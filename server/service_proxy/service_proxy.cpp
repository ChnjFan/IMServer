//
// Created by fan on 24-12-12.
//

#define SERVICE_PROXY_SUB_PORT  5561
#define SERVICE_PROXY_PUB_PORT  5562

#include "zmq.hpp"

int main()
{
    zmq::context_t context(1);

    // 创建PAIR套接字用于监控
    zmq::socket_t monitor(context, ZMQ_PAIR);
    monitor.bind("inproc://monitor");

    zmq::socket_t xsub(context, ZMQ_XSUB);
    zmq::socket_t xpub(context, ZMQ_XPUB);
    xpub.bind("tcp://*:6001");

    // TODO:启动线程监听服务发布消息

    xsub.connect("tcp://localhost:6000");
    zmq_proxy(xsub, xpub, monitor);

    return 0;
}