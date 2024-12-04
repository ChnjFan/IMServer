//
// Created by fan on 24-11-25.
//

#include "TcpConn.h"

Base::TcpConn::TcpConn(const Poco::Net::StreamSocket &socket)
            : Poco::Net::TCPServerConnection(socket)
            , recvMsgBuf(SOCKET_BUFFER_LEN)
            , sendMsgBuf(SOCKET_BUFFER_LEN)
            , connSocket()
            , reactor() {

}

void Base::TcpConn::run() {
    try {
        newConnect();

        // 注册poll事件
        connSocket = socket();
        reactor.addEventHandler(socket(),
                                Poco::Observer<Poco::Runnable, Poco::Net::ReadableNotification>(
                                        *this, reinterpret_cast<void (Runnable::*)(
                                                Poco::Net::ReadableNotification *)>(&TcpConn::onReadable)));
        reactor.addEventHandler(socket(),
                                Poco::Observer<Poco::Runnable, Poco::Net::WritableNotification>(
                                        *this, reinterpret_cast<void (Runnable::*)(
                                                Poco::Net::WritableNotification *)>(&TcpConn::onWritable)));
        reactor.addEventHandler(socket(),
                                Poco::Observer<Poco::Runnable, Poco::Net::ErrorNotification>(
                                        *this, reinterpret_cast<void (Runnable::*)(
                                                Poco::Net::ErrorNotification *)>(&TcpConn::onError)));
        reactor.run();

        reactorClose();
    } catch (Poco::Exception& e) {
        std::cerr << "Error: " << e.displayText() << std::endl;
    }
}

void Base::TcpConn::close() {
    connSocket.shutdown();
    connSocket.close();
    reactor.stop();
}

void Base::TcpConn::send(char *msg, uint32_t len) {
    sendMsgBuf.write(msg, len);
}

void Base::TcpConn::sendPdu(Base::IMPdu &imPdu)  {
    char *msg = new char[imPdu.size()];
    uint32_t len = imPdu.serialize(msg, imPdu.size());
    send(msg, len);
    delete[] msg;
}

void Base::TcpConn::newConnect() {

}

void Base::TcpConn::reactorClose() {

}

void Base::TcpConn::handleRecvMsg() {

}

void Base::TcpConn::handleTcpConnError() {

}

void Base::TcpConn::onReadable(Poco::Net::ReadableNotification *pNotification) {
    Poco::Net::StreamSocket socket = pNotification->socket();

    while (socket.available()) {
        char buffer[SOCKET_BUFFER_LEN] = {0};
        int len = socket.receiveBytes(buffer, sizeof(buffer));
        if (len <= 0)
            break;
        recvMsgBuf.write(buffer, len);
        std::cout << recvMsgBuf.data() << std::endl;
    }

    handleRecvMsg();
}

void Base::TcpConn::onWritable(Poco::Net::WritableNotification *pNotification) {
    Poco::Net::StreamSocket socket = pNotification->socket();

    if (sendMsgBuf.empty())
        return;

    socket.sendBytes(sendMsgBuf.data(), sendMsgBuf.size());
    sendMsgBuf.clear();
}

void Base::TcpConn::onError(Poco::Net::ErrorNotification *pNotification) {
    std::cout << "SessionConn error" << std::endl;

    handleTcpConnError();
}

Base::ByteStream &Base::TcpConn::getRecvMsgBuf() {
    return recvMsgBuf;
}

Base::ByteStream &Base::TcpConn::getSendMsgBuf() {
    return sendMsgBuf;
}


