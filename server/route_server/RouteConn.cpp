//
// Created by fan on 24-11-11.
//

#include <memory>
#include "Poco/Net/SocketReactor.h"
#include "Poco/Runnable.h"
#include "Poco/Timer.h"
#include "RouteConn.h"
#include "HeartBeat.h"
#include "MsgHandler.h"
#include "RouteConnManager.h"

RouteConn::RouteConn(const Poco::Net::StreamSocket &socket) : Poco::Net::TCPServerConnection(socket),
                                                                recvMsgBuf(SOCKET_BUFFER_LEN),
                                                                sendMsgBuf(SOCKET_BUFFER_LEN),
                                                                connSocket(),
                                                                lstTimeStamp() { }

void RouteConn::run() {
    try {
        //TODO:保存连接
        connSocket = socket();
        RouteConnManager::getInstance()->addConn(this);
        Poco::Net::SocketReactor reactor;
        // 注册poll事件
        reactor.addEventHandler(connSocket,
                                Poco::Observer<Poco::Runnable, Poco::Net::ReadableNotification>(
                                        *this, reinterpret_cast<void (Runnable::*)(
                                                Poco::Net::ReadableNotification *)>(&RouteConn::onReadable)));
        reactor.addEventHandler(connSocket,
                                Poco::Observer<Poco::Runnable, Poco::Net::WritableNotification>(
                                        *this, reinterpret_cast<void (Runnable::*)(
                                                Poco::Net::WritableNotification *)>(&RouteConn::onWritable)));
        reactor.addEventHandler(connSocket,
                                Poco::Observer<Poco::Runnable, Poco::Net::ErrorNotification>(
                                        *this, reinterpret_cast<void (Runnable::*)(
                                                Poco::Net::ErrorNotification *)>(&RouteConn::onError)));
        reactor.run();
    } catch (Poco::Exception& e) {
        std::cerr << "Error: " << e.displayText() << std::endl;
    }
}

void RouteConn::close() {
    // TODO: 关闭连接
    std::cout << "Session close" << std::endl;
    connSocket.close();
}

void RouteConn::send(char *msg, uint32_t len) {
    sendMsgBuf.write(msg, len);
}

void RouteConn::sendPdu(IMPdu &imPdu) {
    char *msg = new char[imPdu.size()];
    uint32_t len = imPdu.serialize(msg, imPdu.size());
    send(msg, len);
    delete[] msg;
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
    sendMsgBuf.clear();
}

void RouteConn::onError(Poco::Net::ErrorNotification *pNotification) {
    std::cout << "RouteConn error" << std::endl;
    RouteConnManager::getInstance()->closeConn(this);
}

void RouteConn::recvMsgHandler() {
    while (true) {
        std::shared_ptr<IMPdu> pImPdu = IMPdu::readPdu(recvMsgBuf);
        if (pImPdu == nullptr)
            return;

        MsgHandler::exec(*this, pImPdu);
    }
}

const Poco::Timestamp RouteConn::getLstTimeStamp() const {
    return lstTimeStamp;
}

void RouteConn::updateLsgTimeStamp() {
    lstTimeStamp.update();
}


