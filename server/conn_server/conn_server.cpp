/**
 * @file conn_server.cpp
 * @brief 连接服务器
 */

#include <string>
#include "Poco/ThreadPool.h"
#include "Poco/Util/ServerApplication.h"
#include "Exception.h"
#include "MsgDispatcher.h"
#include "HeartBeatHandler.h"
#include "ApplicationConfig.h"
#include "ServiceProvider.h"

class ConnServer : public Poco::Util::ServerApplication {
protected:
    int main(const std::vector<std::string>& args) override {
        try {
            // 读配置文件
            ServerNet::ApplicationConfig config("conn_server_config.ini");

            // TODO:订阅组件内服务

            // 初始化消息转发器
            MsgDispatcher::init();

            // 定时检测连接心跳
            HeartBeatHandlerImpl heartBeatTask;
            heartBeatTask.start();

            ServerNet::ServiceProvider server;
            // TODO:注册服务
            server.run<MsgDispatcher>(config);

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

private:
    static constexpr int DEFAULT_WORKER_THREAD_NUM = 8;
};

POCO_SERVER_MAIN(ConnServer)