//
// Created by fan on 24-12-7.
//

#include "Poco/Runnable.h"
#include "BaseService.h"

Base::BaseService::BaseService(const char *serviceName, const char *publishPort, const char *requestPort, const char *responsePort)
                               : serviceName(serviceName)
                               , context(1)
                               , reactor()
                               , publishPort(publishPort)
                               , requestPort(requestPort)
                               , responsePort(responsePort)
                               , publishMsgBuf(SOCKET_BUFFER_LEN)
                               , recvMsgBuf(SOCKET_BUFFER_LEN)
                               , sendMsgBuf(SOCKET_BUFFER_LEN) {
    initialize();
}

Base::BaseService::~BaseService() {
    publisher->close();
    puller->close();
    pusher->close();
}

void Base::BaseService::initialize() {
    setupSockets();
    setupReactor();
}

void Base::BaseService::setupSockets() {
    // 服务发布socket（PUB）
    publisher = std::make_unique<zmq::socket_t>(context, zmq::socket_type::pub);
    publisher->bind("tcp://*:" + publishPort);

    // 请求接收socket（PULL）
    puller = std::make_unique<zmq::socket_t>(context, zmq::socket_type::pull);
    puller->bind("tcp://*:" + requestPort);

    // 响应发送socket（PUSH）
    pusher = std::make_unique<zmq::socket_t>(context, zmq::socket_type::push);
    pusher->bind("tcp://*:" + responsePort);
}

void Base::BaseService::setupReactor() {
    ZMQSocketWrapper publisherWrapper(*publisher.get());
    reactor.addEventHandler(publisherWrapper.socket(),
            Poco::Observer<BaseService, Poco::Net::WritableNotification>
                    (*this, reinterpret_cast<void (BaseService::*)(
                            Poco::Net::WritableNotification *)>(&BaseService::onPublish)));

    ZMQSocketWrapper pullerWrapper(*puller.get());
    reactor.addEventHandler(pullerWrapper.socket(),
                            Poco::Observer<BaseService, Poco::Net::ReadableNotification>
                                    (*this, reinterpret_cast<void (BaseService::*)(
                                            Poco::Net::ReadableNotification *)>(&BaseService::onReadable)));

    ZMQSocketWrapper pusherWrapper(*pusher.get());
    reactor.addEventHandler(pusherWrapper.socket(),
                            Poco::Observer<BaseService, Poco::Net::WritableNotification>
                                    (*this, reinterpret_cast<void (BaseService::*)(
                                            Poco::Net::WritableNotification *)>(&BaseService::onWritable)));
}

void Base::BaseService::onPublish(Poco::Net::ReadableNotification *pNotification) {

}

void Base::BaseService::onReadable(Poco::Net::ReadableNotification *pNotification) {

}

void Base::BaseService::onWritable(Poco::Net::ReadableNotification *pNotification) {

}

void Base::BaseService::start() {
    // 服务启动后向客户端发布服务
    publishServiceInfo();
    reactor.run();
}

void Base::BaseService::publishServiceInfo() {

}
