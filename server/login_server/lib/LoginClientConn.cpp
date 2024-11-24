//
// Created by fan on 24-11-24.
//

#include <string>
#include <iostream>
#include "Poco/Net/StreamSocket.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Util/IniFileConfiguration.h"
#include "Poco/AutoPtr.h"
#include "Poco/Runnable.h"
#include "LoginClientConn.h"

LoginClientConn::LoginClientConn(Poco::Net::SocketReactor &reactor, Poco::ThreadPool &threadPool)
                                : reactor(reactor), threadPool(threadPool), recvMsgBuf(SOCKET_BUFFER_LEN), sendMsgBuf(SOCKET_BUFFER_LEN) {
    Poco::AutoPtr<Poco::Util::IniFileConfiguration> pConfig(new Poco::Util::IniFileConfiguration("login_server_config.ini"));
    serverIP = "127.0.0.1";
    serverPort = pConfig->getInt("server.listen_port");
}

void LoginClientConn::run() {
    try {
        Poco::Net::SocketAddress address(serverIP, serverPort);
        pSocket = std::make_shared<Poco::Net::StreamSocket>(address);

        reactor.addEventHandler(*pSocket,
                                  Poco::Observer<Poco::Runnable, Poco::Net::ReadableNotification>(
                                          *this, reinterpret_cast<void (Runnable::*)(
                                                  Poco::Net::ReadableNotification *)>(&LoginClientConn::onReadable)));
        reactor.addEventHandler(*pSocket,
                                  Poco::Observer<Poco::Runnable, Poco::Net::WritableNotification>(
                                          *this, reinterpret_cast<void (Runnable::*)(
                                                  Poco::Net::WritableNotification *)>(&LoginClientConn::onWritable)));
        reactor.addEventHandler(*pSocket,
                                  Poco::Observer<Poco::Runnable, Poco::Net::ErrorNotification>(
                                          *this, reinterpret_cast<void (Runnable::*)(
                                                  Poco::Net::ErrorNotification *)>(&LoginClientConn::onError)));
        reactor.run();
    }
    catch(Poco::Exception& ex) {
        std::cerr << "Error connecting to server: " << ex.displayText() << std::endl;
    }
}

LoginClientConn::~LoginClientConn() {
    pSocket->close();
}

void LoginClientConn::onReadable(Poco::Net::ReadableNotification *pNotification) {
    Poco::Net::StreamSocket socket = pNotification->socket();

    while (socket.available()) {
        char buffer[SOCKET_BUFFER_LEN] = {0};
        int len = socket.receiveBytes(buffer, sizeof(buffer));
        if (len <= 0)
            break;
        recvMsgBuf.write(buffer, len);
    }

    //TODO: 执行客户端可读回调
}

void LoginClientConn::onWritable(Poco::Net::WritableNotification *pNotification) {
    Poco::Net::StreamSocket socket = pNotification->socket();

    if (sendMsgBuf.empty())
        return;

    socket.sendBytes(sendMsgBuf.data(), sendMsgBuf.size());
    sendMsgBuf.clear();
}

void LoginClientConn::onError(Poco::Net::ErrorNotification *pNotification) {
}

