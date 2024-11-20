/**
 * @file route_server.cpp
 * @brief 路由服务器
 */

#include "Poco/Util/ServerApplication.h"
#include "Poco/Util/IniFileConfiguration.h"
#include "Poco/AutoPtr.h"
#include "RouteConn.h"
#include "HeartBeat.h"
#include "MsgHandler.h"
#include <vector>
#include <string>

class RouteServer : public Poco::Util::ServerApplication {
protected:
    int main(const std::vector<std::string>& args) override {
        try {
            readConfig("route_server_config.ini");

            // 注册消息处理回调
            MsgHandlerCallbackMap::getInstance()->registerHandler();
            // 定时检测连接心跳
            HeartBeat checkHeartBeat;

            runServer();

            return Application::EXIT_OK;
        } catch (Poco::Exception& e) {
            std::cerr << e.displayText() << std::endl;
            return Application::EXIT_SOFTWARE;
        }
    }

private:
    void readConfig(const char *file) {
        Poco::AutoPtr<Poco::Util::IniFileConfiguration> pConfig(new Poco::Util::IniFileConfiguration(file));

        listenIP = pConfig->getString("server.listen_ip");
        if (listenIP.empty()) {
            listenIP = "0.0.0.0";
        }

        listenPort = pConfig->getInt("server.listen_port");
        if (0 == listenPort) {
            listenPort = DEFAULT_PORT;
        }
    }

    void runServer() {
        // Set up the TCP server parameters
        Poco::Net::TCPServerParams* pParams = new Poco::Net::TCPServerParams;
        pParams->setMaxQueued(DEFAULT_MAX_CONN); // Maximum number of queued connections
        pParams->setMaxThreads(DEFAULT_THREAD_NUM);  // Maximum number of threads

        // Create the TCP server factory
        Poco::Net::TCPServer server(new RouteConnFactory(), *new Poco::Net::ServerSocket(listenPort), pParams);

        // Start the server
        server.start();
        std::cout << "Server started listen on " << listenIP << ":" << listenPort << std::endl;

        // Wait for termination request
        waitForTerminationRequest();

        server.stop();
    }

private:
    const int DEFAULT_MAX_CONN = 100;
    const int DEFAULT_THREAD_NUM = 16;
    const int DEFAULT_PORT = 1234;

    int listenPort;
    std::string listenIP;
};

POCO_SERVER_MAIN(RouteServer)