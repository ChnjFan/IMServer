//
// Created by fan on 24-12-28.
//

#include "ApplicationConfig.h"

RpcService::ApplicationConfig* pAppConfig = nullptr;

void RpcService::ApplicationConfig::init(std::string configFile) {
    if (nullptr == pAppConfig)
        pAppConfig = new RpcService::ApplicationConfig();
    pAppConfig->readConfig(configFile);
}

RpcService::ApplicationConfig *RpcService::ApplicationConfig::getInstance() {
    if (nullptr == pAppConfig)
        pAppConfig = new RpcService::ApplicationConfig();
    return pAppConfig;
}

void RpcService::ApplicationConfig::destroyInstance() {
    if (nullptr != pAppConfig)
        delete pAppConfig;
}

void RpcService::ApplicationConfig::readConfig(std::string &configFile) {
    pConfig.reset(new Poco::Util::IniFileConfiguration(configFile));
}

Poco::AutoPtr<Poco::Util::IniFileConfiguration> &RpcService::ApplicationConfig::getConfig() {
    return pConfig;
}
