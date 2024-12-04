//
// Created by fan on 24-11-23.
//

#include "Poco/Net/TCPServer.h"
#include "Poco/Util/ServerApplication.h"
#include "Poco/Util/IniFileConfiguration.h"
#include "LoginConn.h"

class AccountServer : public  Poco::Util::ServerApplication {
protected:
    int main(const std::vector<std::string>& args) override {
        try {
            readConfig("login_server_config.ini");

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
        Poco::Net::TCPServer server(new AccountConnFactory(), *new Poco::Net::ServerSocket(listenPort), pParams);

        // Start the server
        server.start();
        std::cout << "Login server started listen on " << listenIP << ":" << listenPort << std::endl;

        // Wait for termination request
        waitForTerminationRequest();

        server.stop();
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