//
// Created by fan on 24-11-11.
//

#include "RouteConn.h"

RouteConn::RouteConn(const Poco::Net::StreamSocket &socket) : Poco::Net::TCPServerConnection(socket),
                                                                recvMsgBuf(SOCKET_BUFFER_LEN) { }

void RouteConn::run() {//run方法返回后会跟客户端断链
    try {
        while (true)
        {
            char buffer[SOCKET_BUFFER_LEN] = {0};
            int len = socket().receiveBytes(buffer, sizeof(buffer));
            if (len <= 0)
                break;
            recvMsgBuf.write(buffer, len);
        }
        recvMsgHandler();
    } catch (Poco::Exception& e) {
        std::cerr << "Error: " << e.displayText() << std::endl;
    }
}

void RouteConn::recvMsgHandler() {
    while (true) {
        std::shared_ptr<IMPdu> pImPdu = IMPdu::readPdu(recvMsgBuf);
        if (pImPdu == nullptr)
            return;
    }
}

