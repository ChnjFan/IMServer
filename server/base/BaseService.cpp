//
// Created by fan on 24-12-7.
//

#include "Poco/Runnable.h"
#include "Poco/JSON/Object.h"
#include "BaseService.h"
#include "zmq.h"
#include "zmq_addon.hpp"

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
                std::vector<zmq::message_t> recv_msgs;

                auto res = zmq::recv_multipart(worker, std::back_inserter(recv_msgs));
                if (!res) continue;

                std::cout << "Recv Request: " << recv_msgs[0].to_string() << std::endl;
                for (auto it = recv_msgs.begin()+1; it != recv_msgs.end(); ++it)
                    std::cout << "Recv Msg: " << (*it).to_string() << std::endl;
                ZMQMessage zmqMsg;
                zmqMsg.setIdentity(recv_msgs[0]);
                zmqMsg.setMsg(recv_msgs[1]);
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

Base::BlockingQueue<Base::ZMQMessage> &Base::BaseWorker::getRecvMsgQueue() {
    return recvMsgQueue;
}

Base::BlockingQueue<Base::ZMQMessage> &Base::BaseWorker::getSendMsgQueue() {
    return sendMsgQueue;
}

void Base::BaseServiceUpdate::setService(Base::BaseService *pBaseService) {
    this->pService = pBaseService;
}

void Base::BaseServiceUpdate::run() {
    updateServiceInfo();
}

void Base::BaseServiceUpdate::updateServiceInfo() {
    Poco::JSON::Object serviceInfo;

    std::string routerEndpoint = pService->getRequestConnEndpoint();
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
                                , publishEndpoint(param.getPublishEndport())
                                , requestEndpoint(param.getRequestEndpoint())
                                , requestConnEndpoint(param.getRequestConnectEndpoint())
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
    publisher.bind(publishEndpoint);
    frontend.bind(requestEndpoint);
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
        zmq::proxy(zmq::socket_ref(zmq::from_handle, frontend.handle()),
                   zmq::socket_ref(zmq::from_handle, backend.handle()));
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

std::string Base::BaseService::getRequestConnEndpoint() const {
    return requestConnEndpoint;
}
