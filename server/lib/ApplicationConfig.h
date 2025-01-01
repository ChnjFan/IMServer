//
// Created by fan on 24-12-28.
//

#ifndef IMSERVER_APPLICATIONCONFIG_H
#define IMSERVER_APPLICATIONCONFIG_H

#include "Poco/Util/IniFileConfiguration.h"

namespace ServerNet {

/**
 * @class ApplicationConfig
 * @brief 应用配置类
 * @note 单例模式，服务上电后应该先初始化配置文件，后续加载会直接读取配置
 */
class ApplicationConfig {
public:
    explicit ApplicationConfig(std::string configFile);

    /**
     * @brief 获取配置
     */
    Poco::AutoPtr<Poco::Util::IniFileConfiguration>& getConfig();

private:
    void readConfig(std::string& configFile);

    Poco::AutoPtr<Poco::Util::IniFileConfiguration> pConfig;
};

}

#endif //IMSERVER_APPLICATIONCONFIG_H
