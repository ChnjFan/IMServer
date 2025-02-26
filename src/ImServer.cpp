/**
 * @file conn_server.cpp
 * @brief 连接服务器
 */

#include <string>
#include "Poco/ThreadPool.h"
#include "Poco/Util/ServerApplication.h"
#include "Exception.h"
#include "MessageService.h"

class ImServer : public Poco::Util::ServerApplication {
protected:
    int main(const std::vector<std::string>& args) override {
        try {
            MessageService server(10001);

            server.start();
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
};

POCO_SERVER_MAIN(ImServer)