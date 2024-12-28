//
// Created by fan on 24-12-28.
//

#ifndef IMSERVER_APPLICATIONCONFIG_H
#define IMSERVER_APPLICATIONCONFIG_H

#include "Poco/Util/IniFileConfiguration.h"

namespace RpcService {

/**
 * @class ApplicationConfig
 * @brief 应用配置类
 * @note 单例模式，服务上电后应该先初始化配置文件，后续加载会直接读取配置
 */
class ApplicationConfig {
public:
    ApplicationConfig() = default;

    /**
     * @brief 初始化配置文件
     * @param configFile 配置文件路径
     */
    static void init(std::string configFile);
    /**
     * @brief 获取配置实例
     */
    static ApplicationConfig* getInstance();
    /**
     * @brief 删除配置实例
     */
    static void destroyInstance();

    /**
     * @brief 读取配置文件
     * @param configFile 配置文件路径
     */
    void readConfig(std::string& configFile);

    /**
     * @brief 获取配置
     * @param configFile 配置文件路径
     */
    Poco::AutoPtr<Poco::Util::IniFileConfiguration>& getConfig();

private:
    Poco::AutoPtr<Poco::Util::IniFileConfiguration> pConfig;
    static ApplicationConfig* pAppConfig;
};

}

#endif //IMSERVER_APPLICATIONCONFIG_H
