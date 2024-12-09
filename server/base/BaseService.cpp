//
// Created by fan on 24-12-7.
//

#include "Poco/Runnable.h"
#include "Poco/JSON/Object.h"
#include "BaseService.h"
#include "zmq.h"

Base::BaseService::BaseService(Base::ServiceParam &param)
                                : context(1)
                                , reactor()
                                , threadPoolSize(param.getThreadPoolSize())
                                , threadPool(param.getServiceName()+"ThreadPool", 1, param.getThreadPoolSize())
                                , serviceName(param.getServiceName())
                                , publishPort(param.getServicePort())
                                , routePort(param.getRoutePort())
                                , publisher(context, zmq::socket_type::pub)
                                , router(context, zmq::socket_type::router)
                                , messageQueue() {
    initialize();
}

Base::BaseService::~BaseService() {
    publisher.close();
    router.close();
    reactor.stop();
}

void Base::BaseService::initialize() {
    try {
        setupSockets();
        setupReactor();
        setupThreadPool();
    }
    catch (const std::exception &e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
    }
}

void Base::BaseService::setupSockets() {
    publisher.bind("tcp://*:" + publishPort);
    router.bind("tcp://*:" + routePort);
}

void Base::BaseService::setupReactor() {
    int fd;
    size_t len = sizeof(fd);
    int ret = zmq_getsockopt(router, ZMQ_FD, &fd, &len);
    if (0 != ret) {
        std::cerr << "zmq_getsockopt error: "<< strerror(errno) << std::endl;
        throw Base::Exception("zmq_getsockopt error");
    }
    reactor.addEventHandler(Poco::Net::Socket::fromFileDescriptor(fd),
                            Poco::Observer<BaseService, Poco::Net::ReadableNotification>
                                    (*this, reinterpret_cast<void (BaseService::*)(
                                            Poco::Net::ReadableNotification *)>(&BaseService::onReadable)));
}

void Base::BaseService::setupThreadPool() {
    // 启动消息处理线程
    for (uint32_t i = 0; i < threadPoolSize; ++i) {
        threadPool.start(*this, serviceName + "_WorkThread_" + std::to_string(i));
    }
}

void Base::BaseService::onReadable(Poco::Net::ReadableNotification *pNotification) {
    std::vector<zmq::message_t> routingParts;
    zmq::message_t message;

    // 接收所有消息帧
    while (true) {
        if (!router.recv(message, zmq::recv_flags::dontwait)) {
            return;
        }

        // 检查是否还有更多消息帧
        int more = router.get(zmq::sockopt::rcvmore);
        if (message.size() > 0) {  // 保存非空消息帧
            routingParts.push_back(std::move(message));
        }

        if (!more) {  // 没有更多消息帧
            break;
        }
    }

    if (routingParts.size() < 2) {  // 至少需要路由ID和消息内容
        return;
    }

    // 最后一帧是实际消息内容
    ByteStream msg_content(routingParts.back().size());
    msg_content.write(static_cast<char*>(routingParts.back().data()), routingParts.back().size());
    routingParts.pop_back();  // 移除消息内容，只保留路由信息

    // 将消息加入队列
    ZMQMessage content(std::move(routingParts), msg_content);
    messageQueue.push(std::move(content));
}

void Base::BaseService::start() {
    // 服务启动后向客户端发布服务
    publishServiceInfo();
    reactor.run();
}

void Base::BaseService::run() {
    messageProcessor();
}

void Base::BaseService::sendResponse(const std::vector<zmq::message_t> &routingParts, Base::ByteStream &response) {
    // 发送路由帧
    for (size_t i = 0; i < routingParts.size(); ++i) {
        zmq::message_t routePart(routingParts[i].data(), routingParts[i].size());
        router.send(routePart, zmq::send_flags::sndmore);
    }

    // 发送消息内容
    zmq::message_t message(response.data(), response.size());
    router.send(message, zmq::send_flags::none);
}

void Base::BaseService::sendResponse(const std::vector<zmq::message_t> &routingParts, char *response, size_t size) {
    // 发送路由帧
    for (size_t i = 0; i < routingParts.size(); ++i) {
        zmq::message_t routePart(routingParts[i].data(), routingParts[i].size());
        router.send(routePart, zmq::send_flags::sndmore);
    }

    // 发送消息内容
    zmq::message_t message(response, size);
    router.send(message, zmq::send_flags::none);
}

void Base::BaseService::publishServiceInfo() {
    Poco::JSON::Object serviceInfo;
    serviceInfo.set("type", "service_info");
    serviceInfo.set("name", serviceName.c_str());
    serviceInfo.set("status", "active");
    serviceInfo.set("timestamp", Poco::DateTimeFormatter::format(Poco::DateTime(), "%Y-%m-%d %H:%M:%S"));

    std::string routerEndpoint = router.get(zmq::sockopt::last_endpoint);
    serviceInfo.set("end_point", routerEndpoint);
    serviceInfo.set("status_updates_port", publishPort);

    std::stringstream ss;
    serviceInfo.stringify(ss);
    std::string jsonString = ss.str();

    // 发布服务信息
    zmq::message_t topic(serviceName.c_str(), serviceName.length());
    publisher.send(topic, zmq::send_flags::sndmore);
    zmq::message_t content(jsonString.data(), jsonString.size());
    publisher.send(content, zmq::send_flags::none);
}

void Base::BaseService::messageProcessor() {

}

Base::BlockingQueue<Base::ZMQMessage> &Base::BaseService::getMessageQueue() {
    return messageQueue;
}
