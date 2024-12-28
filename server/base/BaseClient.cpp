//
// Created by fan on 24-12-9.
//

#include "Poco/JSON/Object.h"
#include "Poco/JSON/Parser.h"
#include "Poco/UUIDGenerator.h"
#include "BaseClient.h"
#include "zmq_addon.hpp"
#include "Exception.h"

Base::BaseClient::BaseClient(std::string& serviceProxyEndPoint, Poco::ThreadPool& threadPool)
                                : serviceProxyEndPoint(serviceProxyEndPoint)
                                , context(1)
                                , subscriber(context, zmq::socket_type::sub)
                                , client(context, zmq::socket_type::dealer)
                                , serviceEndpoint()
                                , subscribeService()
                                , threadPool(threadPool)
                                , mutex() {
    initialize();
}

void Base::BaseClient::initialize() {
    try {
        /* 连接到代理服务器，等待服务发布消息 */
        subscriber.connect(serviceProxyEndPoint);
    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
}

Base::BaseClient::~BaseClient() {
}

void Base::BaseClient::subscribe(std::string &serviceName) {
    subscribeService.insert(std::pair<std::string, zmq::socket_t>(serviceName, zmq::socket_t(context, zmq::socket_type::dealer)));
}

void Base::BaseClient::send(std::string &content) {
    zmq::message_t msg(content.c_str(), content.length());
    std::cout << "Send msg: " << msg.to_string() << std::endl;
    client.send(msg, zmq::send_flags::none);
}

void Base::BaseClient::send(Base::ByteStream &content) {

    zmq::message_t msg(content.data(), content.size());
    client.send(msg, zmq::send_flags::none);
}

void Base::BaseClient::recv(std::vector<zmq::message_t> &message) {
    auto res = zmq::recv_multipart(subscriber, std::back_inserter(message));
    if (!res || message.size() != 2)
        throw Base::Exception("Client recv message error");
}

void Base::BaseClient::start() {
    for (const auto& service : subscribeService) {
        subscriber.set(zmq::sockopt::subscribe, service.first);
    }
    /* 启动一个线程接收服务状态消息 */
    threadPool.start(*this);
}

void Base::BaseClient::startTask(Poco::Runnable &task) {
    threadPool.start(task);
}

void Base::BaseClient::run() {
    //TODO:增加日志记录订阅服务
    while (true) {
        std::vector<zmq::message_t> message;
        auto res = zmq::recv_multipart(subscriber, std::back_inserter(message));
        if(!res || message.size() != 2) {
            assert(0);
        }

        std::cout << message[0].to_string() << ": " << message[1].to_string() << std::endl;
        std::string serviceStatus = message[1].to_string();
        parseServiceUpdate(serviceStatus);

        Poco::Thread::trySleep(100);
    }
}

void Base::BaseClient::parseServiceUpdate(std::string &update) {
    try {
        Poco::JSON::Parser parser;
        auto result = parser.parse(update);
        auto object = result.extract<Poco::JSON::Object::Ptr>();

        std::cout << "Recv Service " +  object->getValue<std::string>("name") + " status update." << std::endl;

        auto type = object->getValue<std::string>("type");
        if (type != "service_info")
            return;
        // 检查是否订阅该服务
        auto serviceName = object->getValue<std::string>("name");
        if (subscribeService.find(serviceName) == subscribeService.end())
            return;

        auto status = object->getValue<std::string>("status");
        if (status == "active") {
            // 服务上线
            Poco::Mutex::ScopedLock lock(mutex);
            serviceEndpoint = object->getValue<std::string>("end_point");
            connectService(serviceName);
        }
        else {
            running = false;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing update: " << e.what() << std::endl;
    }
}

void Base::BaseClient::connectService(std::string& serviceName) {
    client.connect(serviceEndpoint);
    std::cout << "Connect service: " << serviceEndpoint << std::endl;
}


