//
// Created by fan on 24-12-28.
//

#include "ApplicationConfig.h"

ServerNet::ApplicationConfig::ApplicationConfig(std::string configFile) {
    readConfig(configFile);
}

void ServerNet::ApplicationConfig::readConfig(std::string &configFile) {
    pConfig.reset(new Poco::Util::IniFileConfiguration(configFile));
}

Poco::AutoPtr<Poco::Util::IniFileConfiguration> &ServerNet::ApplicationConfig::getConfig() {
    return pConfig;
}
