//
// Created by fan on 24-12-9.
//

#include "Poco/JSON/Object.h"
#include "Poco/JSON/Parser.h"
#include "BaseClient.h"

Base::BaseClient::BaseClient(std::string& serviceIP, uint32_t servicePort)
                                : serviceIP(serviceIP)
                                , servicePort(servicePort)
                                , context(1)
                                , subscriber(context, zmq::socket_type::sub)
                                , dealer(context, zmq::socket_type::dealer)
                                , dealerPort(0)
                                , listener("ServiceListener")
                                , mutex() {
    initialize();
}

void Base::BaseClient::initialize() {
    try {
        /* 启动监听线程，监听服务状态 */
        listener.start(*this);
        subscriber.connect("tcp://localhost:" + std::to_string(servicePort));
        running = true;
    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        running = false;
    }
}

Base::BaseClient::~BaseClient() {
    running = false;
    // TODO:停止线程
    if (listener.isRunning()) {
        listener.join();
    }
}

void Base::BaseClient::run() {
    std::cout << "Start listener thread: listening service" << std::endl;
    while (true) {
        zmq::message_t update;
        if (subscriber.recv(update, zmq::recv_flags::dontwait)) {
            std::string updateStr(static_cast<char *>(update.data()), update.size());
            parseServiceUpdate(updateStr);
        }
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
                running = true;
                auto endpoint = object->getValue<std::string>("end_point");
                dealer.connect(endpoint);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing update: " << e.what() << std::endl;
    }
}



