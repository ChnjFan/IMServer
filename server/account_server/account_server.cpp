//
// Created by fan on 24-11-23.
//

#include "Poco/Util/ServerApplication.h"
#include "Poco/Util/IniFileConfiguration.h"
#include "AccountService.h"

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

        publishEndpoint = pConfig->getString("server.publish_endpoint");
        requestBindEndpoint = pConfig->getString("server.request_bind_endpoint");
        requestConnectEndpoint = pConfig->getString("server.request_connect_endpoint");
    }

    void runServer() {
        Base::ServiceParam param("AccountService",
                                 publishEndpoint,
                                 requestBindEndpoint,
                                 requestConnectEndpoint,
                                 8, 100);
        AccountService service(param);
        service.start();
    }

private:

    std::string requestBindEndpoint;
    std::string requestConnectEndpoint;
    std::string publishEndpoint;
};

POCO_SERVER_MAIN(AccountServer)