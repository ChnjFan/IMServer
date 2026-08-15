//
// Created by Fan on 2026/7/2.
//

#ifndef IMSERVER_RESOURCECONFIG_H
#define IMSERVER_RESOURCECONFIG_H

#include "ConfigMgr.h"

// Upload constants
#define RESOURCE_ID_PREFIX       "res_"
#define RESOURCE_ID_HEX_LEN      8
#define THUMB_SUFFIX             "_thumb"
#define MAX_FILE_SIZE_DEFAULT    (50 * 1024 * 1024)  // 50MB
#define UPLOAD_TIMEOUT_MINUTES   30

struct ImageProcessConfig {
    int  maxDimension = 2048;
    int  quality = 85;
    int  thumbSize = 200;
    bool enableCompress = true;
    bool enableThumb = true;
    int64_t maxFileSize = MAX_FILE_SIZE_DEFAULT;

    static ImageProcessConfig fromConfigMgr() {
        ImageProcessConfig cfg;
        auto& config = ConfigMgr::getInstance();
        if (config["ImageProcess"].hasValue("MaxDimension")) {
            cfg.maxDimension = std::stoi(config["ImageProcess"]["MaxDimension"]);
        }
        if (config["ImageProcess"].hasValue("Quality")) {
            cfg.quality = std::stoi(config["ImageProcess"]["Quality"]);
        }
        if (config["ImageProcess"].hasValue("ThumbSize")) {
            cfg.thumbSize = std::stoi(config["ImageProcess"]["ThumbSize"]);
        }
        if (config["ImageProcess"].hasValue("MaxFileSize")) {
            cfg.maxFileSize = std::stoll(config["ImageProcess"]["MaxFileSize"]);
        }
        return cfg;
    }
};

#endif //IMSERVER_RESOURCECONFIG_H