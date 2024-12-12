/**
 * @file conn_server.cpp
 * @brief 连接服务器
 */

#include <string>
#include "Poco/ThreadPool.h"
#include "Poco/Util/ServerApplication.h"
#include "Poco/Util/IniFileConfiguration.h"
#include "Poco/Net/TCPServerParams.h"
#include "Poco/Net/TCPServer.h"
#include "Poco/Net/IPAddress.h"
#include "Exception.h"
#include "SessionConn.h"
#include "MsgDispatcher.h"
#include "HeartBeatHandler.h"
#include "BaseClient.h"
#include "String.h"

class ConnServer : public Poco::Util::ServerApplication {
protected:
    int main(const std::vector<std::string>& args) override {
        try {
            readConfig("conn_server_config.ini");
            Poco::ThreadPool threadPool;

            // 注册消息处理回调
            MsgHandlerCallbackMap::getInstance()->registerHandler();
            // 定时检测连接心跳
            HeartBeatHandlerImpl heartBeatTask;
            heartBeatTask.start();
            // 订阅 account_server 服务
            Base::BaseClient accountClient(serviceProxyEndPoint, threadPool);
            accountClient.subscribe(serviceName[0]);
            accountClient.start();

            runServer();

            return Application::EXIT_OK;
        } catch (Poco::Exception& e) {
            std::cerr << e.displayText() << std::endl;
            return Application::EXIT_SOFTWARE;
        } catch (Base::Exception& e) {
            std::cerr << e.what() << std::endl;
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

        serviceProxyEndPoint = pConfig->getString("subscribe.proxy_endpoint");
        std::string services = pConfig->getString("subscribe.service");
        Base::String::split(serviceName, services, ',');
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
    std::string serviceProxyEndPoint;
    std::vector<std::string> serviceName;
    Poco::Timespan::TimeDiff heartbeatCheckTime;
};

POCO_SERVER_MAIN(ConnServer)