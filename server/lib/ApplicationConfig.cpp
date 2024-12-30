//
// Created by fan on 24-12-28.
//

#include "ApplicationConfig.h"

ServerNet::ApplicationConfig* pAppConfig = nullptr;

void ServerNet::ApplicationConfig::init(std::string configFile) {
    if (nullptr == pAppConfig)
        pAppConfig = new ServerNet::ApplicationConfig();
    pAppConfig->readConfig(configFile);
}

ServerNet::ApplicationConfig *ServerNet::ApplicationConfig::getInstance() {
    if (nullptr == pAppConfig)
        pAppConfig = new ServerNet::ApplicationConfig();
    return pAppConfig;
}

void ServerNet::ApplicationConfig::destroyInstance() {
    if (nullptr != pAppConfig)
        delete pAppConfig;
}

void ServerNet::ApplicationConfig::readConfig(std::string &configFile) {
    pConfig.reset(new Poco::Util::IniFileConfiguration(configFile));
}

Poco::AutoPtr<Poco::Util::IniFileConfiguration> &ServerNet::ApplicationConfig::getConfig() {
    return pConfig;
}
