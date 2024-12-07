//
// Created by fan on 24-11-23.
//

#include "Poco/Util/ServerApplication.h"
#include "Poco/Util/IniFileConfiguration.h"

class AccountServer : public  Poco::Util::ServerApplication {
protected:
    int main(const std::vector<std::string>& args) override {
        try {
            readConfig("account_server_config.ini");

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
        // TODO: 向客户端发布服务，然后等待客户端请求。
    }

private:
    static constexpr int DEFAULT_MAX_CONN = 100;
    static constexpr int DEFAULT_THREAD_NUM = 16;
    static constexpr int DEFAULT_PORT = 1235;
    static constexpr Poco::Timespan::TimeDiff DEFAULT_HEARTBEAT_TIME = 5*60*1000;

    int listenPort;
    std::string listenIP;
};

POCO_SERVER_MAIN(AccountServer)