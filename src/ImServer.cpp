/**
 * @file conn_server.cpp
 * @brief 连接服务器
 */

#include "ImServer.h"

unsigned short ServerHandle::port_ = 10001;

void ServerHandle::start() {
    reactor.run();
}

void ServerHandle::setTask(Base::MessagePtr &message) {
    task.push(message);
}

Base::MessagePtr ServerHandle::getTask() {
    Base::MessagePtr message;
    task.tryPop(message);
    return message;
}

Base::MessagePtr ServerHandle::getTask(long milliseconds) {
    Base::MessagePtr message;
    task.tryPopFor(message, milliseconds);
    return message;
}

Base::MessagePtr ServerHandle::getResponse(long milliseconds) {
    Base::MessagePtr message;
    response.tryPopFor(message, milliseconds);
    return message;
}

void ServerHandle::sendResponse(Base::MessagePtr &message) {
    response.push(message);
}

void startServer(unsigned short port)
{
    ServerHandle::port_ = port;
    ServerHandle& serverHandle = ServerHandle::getInstance();
    serverHandle.start();
}

class ImServer : public Poco::Util::ServerApplication {
protected:
    int main(const std::vector<std::string>& args) override {
        try {
            startServer(10001);

            waitForTerminationRequest();

            return Application::EXIT_OK;
        } catch (Poco::Exception& e) {
            std::cerr << e.displayText() << std::endl;
            return Application::EXIT_SOFTWARE;
        } catch (Base::Exception& e) {
            std::cerr << e.what() << std::endl;
            return Application::EXIT_SOFTWARE;
        }
    }
};

POCO_SERVER_MAIN(ImServer)