//
// Created by Fan on 2026/5/4.
//

#ifndef IMSERVER_CONFIGMGR_H
#define IMSERVER_CONFIGMGR_H

#include <unordered_map>
#include <string>

struct SectionInfo {
    SectionInfo() = default;
    ~SectionInfo() { section_data_.clear(); }

    SectionInfo(const SectionInfo& other) {
        this->section_data_ = other.section_data_;
    }

    SectionInfo& operator=(const SectionInfo& other) {
        if (&other == this) {
            return *this;
        }
        this->section_data_ = other.section_data_;
        return *this;
    }

    std::string operator[](const std::string& key) {
        if (section_data_.find(key) == section_data_.end()) {
            return "";
        }
        return section_data_[key];
    }

    std::unordered_map<std::string, std::string> section_data_;
};

class ConfigMgr {
public:
    ~ConfigMgr() {
        configMap_.clear();
    }

    static ConfigMgr& getInstance() {
        static ConfigMgr configMgr; // 静态变量只初始化一次，线程安全
        return configMgr;
    }

    ConfigMgr(const ConfigMgr& other) = delete;
    ConfigMgr& operator=(const ConfigMgr& other) = delete;
    SectionInfo operator[](const std::string& key);
    std::string getValue(const std::string& key, const std::string& section);

private:
    ConfigMgr();
    std::unordered_map<std::string, SectionInfo> configMap_;
};

#endif //IMSERVER_CONFIGMGR_H