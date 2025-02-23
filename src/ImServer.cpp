/**
 * @file conn_server.cpp
 * @brief 连接服务器
 */

#include <string>
#include "Poco/ThreadPool.h"
#include "Poco/Util/ServerApplication.h"
#include "Exception.h"

class ImServer : public Poco::Util::ServerApplication {
protected:
    int main(const std::vector<std::string>& args) override {
        try {

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

POCO_SERVER_MAIN(ImServer)