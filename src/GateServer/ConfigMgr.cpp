//
// Created by Fan on 2026/5/4.
//

#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include "ConfigMgr.h"

ConfigMgr::ConfigMgr() {
    const boost::filesystem::path currentDir = boost::filesystem::current_path();
    const auto configFilePath = currentDir / boost::filesystem::path("config.ini");
    std::cout << "Loading Config " << configFilePath.string() << std::endl;

    boost::property_tree::ptree pt;
    boost::property_tree::read_ini(configFilePath.string(), pt);

    for (const auto& [sectionName, sectionTree] : pt) {
        std::map<std::string, std::string> sectionData;
        for (const auto& [key, value] : sectionTree) {
            sectionData[key] = value.get_value<std::string>();
        }
        SectionInfo sectionInfo;
        sectionInfo.section_data_ = sectionData;
        configMap_[sectionName] = sectionInfo;
    }

    // TODO:日志记录加载配置内容
    for (auto& [sectionName, sectionData] : configMap_) {
        std::cout << "[" << sectionName  << "]" << std::endl;
        for (const auto& [key, value] : sectionData.section_data_) {
            std::cout << key << ": " << value << std::endl;
        }
    }
}

ConfigMgr::ConfigMgr(const ConfigMgr &other) {
    this->configMap_ = other.configMap_;
}

ConfigMgr & ConfigMgr::operator=(const ConfigMgr &other) {
    if (this == &other) {
        return *this;
    }
    this->configMap_ = other.configMap_;
    return *this;
}

SectionInfo ConfigMgr::operator[](const std::string &key) {
    if (configMap_.find(key) == configMap_.end()) {
        return {};
    }
    return configMap_[key];
}
