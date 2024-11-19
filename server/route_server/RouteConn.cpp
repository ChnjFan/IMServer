//
// Created by fan on 24-11-11.
//

#include "Poco/Net/SocketReactor.h"
#include "Poco/Runnable.h"
#include "Poco/Timer.h"
#include "RouteConn.h"
#include "HeartBeat.h"


RouteConn::RouteConn(const Poco::Net::StreamSocket &socket) : Poco::Net::TCPServerConnection(socket),
                                                                recvMsgBuf(SOCKET_BUFFER_LEN),
                                                                sendMsgBuf(SOCKET_BUFFER_LEN) { }

void RouteConn::run() {
    try {
        HeartBeat heartBeat(socket());//默认1分钟发送心跳包
        Poco::Net::SocketReactor reactor;

        // 注册poll事件
        reactor.addEventHandler(socket(),
                                Poco::Observer<Poco::Runnable, Poco::Net::ReadableNotification>(
                                        *this, reinterpret_cast<void (Runnable::*)(
                                                Poco::Net::ReadableNotification *)>(&RouteConn::onReadable)));
        reactor.addEventHandler(socket(),
                                Poco::Observer<Poco::Runnable, Poco::Net::WritableNotification>(
                                        *this, reinterpret_cast<void (Runnable::*)(
                                                Poco::Net::WritableNotification *)>(&RouteConn::onWritable)));
        reactor.addEventHandler(socket(),
                                Poco::Observer<Poco::Runnable, Poco::Net::ErrorNotification>(
                                        *this, reinterpret_cast<void (Runnable::*)(
                                                Poco::Net::ErrorNotification *)>(&RouteConn::onError)));
        reactor.run();
    } catch (Poco::Exception& e) {
        std::cerr << "Error: " << e.displayText() << std::endl;
    }
}

void RouteConn::onReadable(Poco::Net::ReadableNotification *pNotification) {
    Poco::Net::StreamSocket socket = pNotification->socket();

    while (socket.available()) {
        char buffer[SOCKET_BUFFER_LEN] = {0};
        int len = socket.receiveBytes(buffer, sizeof(buffer));
        if (len <= 0)
            break;
        recvMsgBuf.write(buffer, len);
        std::cout << recvMsgBuf.data() << std::endl;
    }

    recvMsgHandler();
}

void RouteConn::onWritable(Poco::Net::WritableNotification *pNotification) {
    Poco::Net::StreamSocket socket = pNotification->socket();

    if (sendMsgBuf.empty())
        return;

    socket.sendBytes(sendMsgBuf.data(), sendMsgBuf.size());
}

void RouteConn::onError(Poco::Net::ErrorNotification *pNotification) {
    Poco::Net::StreamSocket socket = pNotification->socket();

    std::cout << "RouteConn error" << std::endl;

    socket.close();
}

void RouteConn::recvMsgHandler() {
    while (true) {
        std::shared_ptr<IMPdu> pImPdu = IMPdu::readPdu(recvMsgBuf);
        if (pImPdu == nullptr)
            return;
    }
}

