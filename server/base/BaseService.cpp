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

                std::cout << "Recv Request: " << identity << std::endl;
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

void Base::BaseServiceUpdate::setService(Base::BaseService *pBaseService) {
    this->pService = pBaseService;
}

void Base::BaseServiceUpdate::run() {
    updateServiceInfo();
}

void Base::BaseServiceUpdate::updateServiceInfo() {
    Poco::JSON::Object serviceInfo;

    std::string routerEndpoint = pService->getRouterEndpoint();
    serviceInfo.set("end_point", routerEndpoint);
    serviceInfo.set("timestamp", Poco::DateTimeFormatter::format(Poco::DateTime(), "%Y-%m-%d %H:%M:%S"));
    serviceInfo.set("status", "active");
    serviceInfo.set("name", pService->getServiceName());
    serviceInfo.set("type", "service_info");

    std::stringstream ss;
    serviceInfo.stringify(ss);
    std::string jsonString = ss.str();

    // 发布服务信息
    std::cout << "Update service info" << std::endl;
    pService->update(pService->getServiceName(), zmq::send_flags::sndmore);
    pService->update(jsonString, zmq::send_flags::none);
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
    // TODO:服务启动后定时向客户端推送状态，设置1s更新一次
    serviceUpdate.setService(this);
    timer.schedule(&serviceUpdate, 0, 3000);
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

void Base::BaseService::update(const std::string &content, zmq::send_flags flag) {
    zmq::message_t msg(content.c_str(), content.length());
    publisher.send(msg, flag);
}

const std::string &Base::BaseService::getServiceName() const {
    return serviceName;
}

std::string Base::BaseService::getRouterEndpoint() const {
    return frontend.get(zmq::sockopt::last_endpoint);
}

const std::string &Base::BaseService::getPublishPort() const {
    return publishPort;
}
