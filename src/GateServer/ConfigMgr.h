//
// Created by Fan on 2026/5/4.
//

#ifndef IMSERVER_CONFIGMGR_H
#define IMSERVER_CONFIGMGR_H

#include <map>
#include <string>

#include "Singleton.h"

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

    std::map<std::string, std::string> section_data_;
};

class ConfigMgr {
public:
    ConfigMgr();

    ~ConfigMgr() {
        configMap_.clear();
    }

    ConfigMgr(const ConfigMgr& other);
    ConfigMgr& operator=(const ConfigMgr& other);
    SectionInfo operator[](const std::string& key);

private:
    std::map<std::string, SectionInfo> configMap_;
};

extern ConfigMgr gConfigMgr;

#endif //IMSERVER_CONFIGMGR_H