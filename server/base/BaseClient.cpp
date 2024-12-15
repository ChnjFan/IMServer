//
// Created by fan on 24-12-9.
//

#include "Poco/JSON/Object.h"
#include "Poco/JSON/Parser.h"
#include "Poco/UUIDGenerator.h"
#include "BaseClient.h"
#include "zmq_addon.hpp"

Base::BaseClient::BaseClient(std::string& serviceProxyEndPoint, Poco::ThreadPool& threadPool)
                                : serviceProxyEndPoint(serviceProxyEndPoint)
                                , context(1)
                                , subscriber(context, zmq::socket_type::sub)
                                , clientSocket(context, zmq::socket_type::dealer)
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
    running = false;
}

void Base::BaseClient::subscribe(std::string &serviceName) {
    subscribeService.push_back(serviceName);
}

void Base::BaseClient::send(std::string &content) {
    if (!running) {
        return;
    }

    zmq::message_t msg(content.c_str(), content.length());
    std::cout << "Send msg: " << msg.to_string() << std::endl;
    clientSocket.send(msg, zmq::send_flags::none);
}

void Base::BaseClient::start() {
    for (const auto& service : subscribeService) {
        subscriber.set(zmq::sockopt::subscribe, service);
    }
    /* 启动一个线程接收处理服务发布消息 */
    threadPool.start(*this);
}

void Base::BaseClient::run() {
    std::cout << "Start listener thread: listening service" << std::endl;
    while (true) {
        zmq::message_t message;
        subscriber.recv(message);
        std::string topic(static_cast<char *>(message.data()), message.size());
        subscriber.recv(message);
        std::string data(static_cast<char *>(message.data()), message.size());
        std::cout << topic << ": " << data << std::endl;
        parseServiceUpdate(data);

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
        if (type == "service_info") {
            auto status = object->getValue<std::string>("status");
            if (status == "active") {
                Poco::Mutex::ScopedLock lock(mutex);
                serviceEndpoint = object->getValue<std::string>("end_point");
                connectService();
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing update: " << e.what() << std::endl;
    }
}

void Base::BaseClient::connectService() {
    if (running)
    {
        return;
    }
    clientSocket.connect(serviceEndpoint);
    std::cout << "Connect service: " << serviceEndpoint << std::endl;
    running = true;

    //TODO:启动一个线程用来接收应答消息
}


