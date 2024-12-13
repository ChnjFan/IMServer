//
// Created by fan on 24-12-7.
//

#include "Poco/Runnable.h"
#include "Poco/JSON/Object.h"
#include "BaseService.h"
#include "zmq.h"

void Base::BaseWorker::run()  {
    worker.connect("inproc://backend");

    zmq::pollitem_t items[] = {
            { worker.handle(), 0, ZMQ_POLLIN | ZMQ_POLLOUT | ZMQ_POLLERR, 0 }
    };

    std::chrono::milliseconds timeout(1000); // 1000 毫秒
    try {
        while (true) {
            zmq::poll(items, 1, timeout);

            if (items[0].revents & ZMQ_POLLIN) {
                zmq::message_t identity;
                zmq::message_t msg;

                auto res = worker.recv(identity, zmq::recv_flags::none);
                if (!res) continue;
                res = worker.recv(msg, zmq::recv_flags::none);
                if (!res) continue;

                ZMQMessage zmqMsg;
                zmqMsg.setIdentity(identity);
                zmqMsg.setMsg(msg);
                recvMsgQueue.push(zmqMsg);

                onReadable();
            }
            else if (items[0].revents & ZMQ_POLLOUT) {
                ZMQMessage zmqMsg;
                if (sendMsgQueue.tryPop(zmqMsg)) {
                    worker.send(zmqMsg.getIdentity(), zmq::send_flags::sndmore);
                    worker.send(zmqMsg.getMsg(), zmq::send_flags::none);
                }
            }
            else if (items[0].revents & ZMQ_POLLERR) {
                onError();
            }
        }
    }
    catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
    }
}

void Base::BaseWorker::onReadable() {

}

void Base::BaseWorker::onError() {

}

Base::BaseService::BaseService(Base::ServiceParam &param)
                                : context(1)
                                , threadPoolSize(param.getThreadPoolSize())
                                , threadPool(param.getServiceName()+"ThreadPool", 1, param.getThreadPoolSize())
                                , serviceName(param.getServiceName())
                                , publishPort(param.getServicePort())
                                , routePort(param.getRoutePort())
                                , publisher(context, zmq::socket_type::pub)
                                , frontend(context, zmq::socket_type::router)
                                , backend(context, zmq::socket_type::dealer) {
    initialize();
}

Base::BaseService::~BaseService() {
    publisher.close();
    frontend.close();
    backend.close();
}

void Base::BaseService::initialize() {
    try {
        setupSockets();
    }
    catch (const std::exception &e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
    }
}

void Base::BaseService::setupSockets() {
    publisher.bind("tcp://*:" + publishPort);
    frontend.bind("tcp://*:" + routePort);
    backend.bind("inproc://backend");
}

void Base::BaseService::start(Base::BaseWorker &worker) {
    // TODO:服务启动后定时向客户端推送状态
    int count = 0;
    while (count < 60)
    {
        publishServiceInfo();
        sleep(1);
        count++;
    }
    startWorkThread(worker);

}

void Base::BaseService::startWorkThread(Base::BaseWorker &worker) {
    // 启动消息处理线程
    threadPool.start(worker, serviceName + "_WorkThread");

    try {
        zmq::proxy(frontend, backend);
    }
    catch (std::exception &e) {}
}

void Base::BaseService::publishServiceInfo() {
    Poco::JSON::Object serviceInfo;
    serviceInfo.set("type", "service_info");
    serviceInfo.set("name", serviceName.c_str());
    serviceInfo.set("status", "active");
    serviceInfo.set("timestamp", Poco::DateTimeFormatter::format(Poco::DateTime(), "%Y-%m-%d %H:%M:%S"));

    std::string routerEndpoint = frontend.get(zmq::sockopt::last_endpoint);
    serviceInfo.set("end_point", routerEndpoint);
    serviceInfo.set("status_updates_port", publishPort);

    std::stringstream ss;
    serviceInfo.stringify(ss);
    std::string jsonString = ss.str();

    // 发布服务信息
    std::cout << "Publish" << std::endl;
    zmq::message_t topic(serviceName.c_str(), serviceName.length());
    publisher.send(topic, zmq::send_flags::sndmore);
    zmq::message_t content(jsonString.data(), jsonString.size());
    publisher.send(content, zmq::send_flags::none);
}
