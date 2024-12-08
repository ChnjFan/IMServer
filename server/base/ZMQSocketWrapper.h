//
// Created by fan on 24-12-7.
//

#ifndef IMSERVER_ZMQSOCKETWRAPPER_H
#define IMSERVER_ZMQSOCKETWRAPPER_H

#include "Poco/Net/Socket.h"
#include "Poco/Net/StreamSocket.h"
#include "zmq.hpp"
#include "Exception.h"

namespace Base {

/**
 * @class FDSocketImpl
 * @brief 自定义Socket实现，用于包装文件描述符
 */
class FDSocketImpl : public Poco::Net::SocketImpl {
public:
    FDSocketImpl(int fd) : Poco::Net::SocketImpl(fd) {}
};

/**
 * @class FDSocket
 * @brief 自定义Socket类，用于包装SocketImpl
 */
class FDSocket : public Poco::Net::Socket {
public:
    FDSocket(FDSocketImpl* impl) : Poco::Net::Socket(impl) {}
};

/**
 * @class ZMQSocketWrapper
 * @brief ZMQ Socket 包装类，用于Poco Reactor
 */
class ZMQSocketWrapper {
public:
    ZMQSocketWrapper(zmq::socket_t& socket) : socket_(socket) {
        // 获取原生socket指针
        void* native_socket = socket.handle();

        // 获取文件描述符
        int fd;
        size_t size = sizeof(fd);
        int rc = zmq_getsockopt(native_socket, ZMQ_FD, &fd, &size);
        if (rc != 0) {
            throw std::runtime_error("Failed to get socket file descriptor");
        }

        // 创建Poco socket
        auto pImpl = new FDSocketImpl(fd);
        pocoSocket_.reset(new FDSocket(pImpl));
        delete pImpl;
    }

    Poco::Net::Socket& socket() { return *pocoSocket_; }
    zmq::socket_t& zmqSocket() { return socket_; }

private:
    zmq::socket_t& socket_;
    std::unique_ptr<Poco::Net::Socket> pocoSocket_;
};

}

#endif //IMSERVER_ZMQSOCKETWRAPPER_H
