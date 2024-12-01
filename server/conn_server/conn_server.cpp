/**
 * @file route_server.cpp
 * @brief 路由服务器
 */

#include <vector>
#include <string>
#include "Poco/AutoPtr.h"
#include "Poco/Util/ServerApplication.h"
#include "Poco/Util/IniFileConfiguration.h"
#include "SessionConn.h"
#include "MsgDispatch.h"
#include "HeartBeatHandler.h"
#include "LoginClientConn.h"
#include "LoginServerResult.h"

class RouteServer : public Poco::Util::ServerApplication {
protected:
    int main(const std::vector<std::string>& args) override {
        try {
            readConfig("route_server_config.ini");
            Poco::Net::SocketReactor reactor;
            Poco::ThreadPool threadPool;

            // 注册消息处理回调
            MsgHandlerCallbackMap::getInstance()->registerHandler();
            // 定时检测连接心跳
            HeartBeatHandler heartBeatTask;
            Poco::Util::Timer timer;
            timer.schedule(&heartBeatTask, heartbeatCheckTime, heartbeatCheckTime);
            // 订阅login_server服务
            registerLoginServer(reactor, threadPool);

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

        heartbeatCheckTime = pConfig->getInt("server.heartbeat_check_time");
        if (0 == heartbeatCheckTime) {
            heartbeatCheckTime = DEFAULT_HEARTBEAT_TIME;
        }
    }

    void registerLoginServer(Poco::Net::SocketReactor& reactor, Poco::ThreadPool& threadPool) {
        LoginServerResult::getInstance()->registerLoginServer(reactor, threadPool);
    }

    void runServer() {
        // Set up the TCP server parameters
        Poco::Net::TCPServerParams* pParams = new Poco::Net::TCPServerParams;
        pParams->setMaxQueued(DEFAULT_MAX_CONN); // Maximum number of queued connections
        pParams->setMaxThreads(DEFAULT_THREAD_NUM);  // Maximum number of threads

        // Create the TCP server factory
        Poco::Net::TCPServer server(new SessionConnFactory(), *new Poco::Net::ServerSocket(listenPort), pParams);

        // Start the server
        server.start();
        std::cout << "Server started listen on " << listenIP << ":" << listenPort << std::endl;

        // Wait for termination request
        waitForTerminationRequest();

        server.stop();
    }

private:
    static constexpr int DEFAULT_MAX_CONN = 100;
    static constexpr int DEFAULT_THREAD_NUM = 16;
    static constexpr int DEFAULT_PORT = 1234;
    static constexpr Poco::Timespan::TimeDiff DEFAULT_HEARTBEAT_TIME = 5*60*1000;

    int listenPort;
    std::string listenIP;
    Poco::Timespan::TimeDiff heartbeatCheckTime;
};

POCO_SERVER_MAIN(RouteServer)