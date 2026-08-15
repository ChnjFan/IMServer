//
// Created by Fan on 2026/7/2.
//

#ifndef IMSERVER_RESOURCEMETA_H
#define IMSERVER_RESOURCEMETA_H

#include <cstdint>
#include <optional>
#include <string>

#include <json/json.h>
#include <jdbc/cppconn/resultset.h>

#include "const.h"

enum class ResourceType : uint8_t {
    IMAGE = 1,
    FILE  = 2,
    VOICE = 3,
    VIDEO = 4,
    OTHER = 5,
};

enum class ResourceStatus : uint8_t {
    NORMAL    = 0,
    UPLOADING = 1,
    DELETED   = 2,
};

struct ResourceMeta {
    std::string resourceId;
    std::string convId;
    int         uploaderUid = -1;
    std::string md5;
    int64_t     fileSize = 0;
    std::string fileName;
    std::string filePath;
    std::string thumbPath;
    uint8_t     resourceType = 0;
    uint8_t     status = 0;
    int         referenceCount = -1;
    int         width = -1;
    int         height = -1;
    int         duration = -1;
    std::string createTime;

    void fromJson(Json::Value& value);
    void toJson(Json::Value& value) const;
    void fromResultSet(const std::shared_ptr<sql::ResultSet>& result);
    static ResourceMeta fromResultListSet(const std::shared_ptr<sql::ResultSet>& result);
};

inline void ResourceMeta::fromJson(Json::Value& value) {
    if (value.isMember("resource_id") && !value["resource_id"].isNull())
        resourceId = value["resource_id"].asString();
    if (value.isMember("conv_id") && !value["conv_id"].isNull())
        convId = value["conv_id"].asString();
    if (value.isMember("uploader_uid") && !value["uploader_uid"].isNull())
        uploaderUid = std::stoi(value["uploader_uid"].asString());
    if (value.isMember("md5") && !value["md5"].isNull())
        md5 = value["md5"].asString();
    if (value.isMember("file_size") && !value["file_size"].isNull())
        fileSize = value["file_size"].asInt64();
    if (value.isMember("file_name") && !value["file_name"].isNull())
        fileName = value["file_name"].asString();
    if (value.isMember("file_path") && !value["file_path"].isNull())
        filePath = value["file_path"].asString();
    if (value.isMember("thumb_path") && !value["thumb_path"].isNull())
        thumbPath = value["thumb_path"].asString();
    if (value.isMember("resource_type") && !value["resource_type"].isNull())
        resourceType = static_cast<uint8_t>(value["resource_type"].asUInt());
    if (value.isMember("status") && !value["status"].isNull())
        status = static_cast<uint8_t>(value["status"].asUInt());
    if (value.isMember("reference_count") && !value["reference_count"].isNull())
        referenceCount = value["reference_count"].asInt();
    if (value.isMember("width") && !value["width"].isNull())
        width = value["width"].asInt();
    if (value.isMember("height") && !value["height"].isNull())
        height = value["height"].asInt();
    if (value.isMember("duration") && !value["duration"].isNull())
        duration = value["duration"].asInt();
    if (value.isMember("create_time") && !value["create_time"].isNull())
        createTime = value["create_time"].asString();
}

inline void ResourceMeta::toJson(Json::Value& value) const {
    if (!resourceId.empty()) value["resource_id"] = resourceId;
    if (!convId.empty()) value["conv_id"] = convId;
    if (uploaderUid >= 0) value["uploader_uid"] = std::to_string(uploaderUid);
    if (!md5.empty()) value["md5"] = md5;
    if (fileSize > 0) value["file_size"] = static_cast<Json::Int64>(fileSize);
    if (!fileName.empty()) value["file_name"] = fileName;
    if (!filePath.empty()) value["file_path"] = filePath;
    if (!thumbPath.empty()) value["thumb_path"] = thumbPath;
    if (resourceType > 0) value["resource_type"] = resourceType;
    value["status"] = status;
    if (referenceCount >= 0) value["reference_count"] = referenceCount;
    if (width > 0) value["width"] = width;
    if (height > 0) value["height"] = height;
    if (duration > 0) value["duration"] = duration;
    if (!createTime.empty()) value["create_time"] = createTime;
}

inline void ResourceMeta::fromResultSet(const std::shared_ptr<sql::ResultSet>& result) {
    resourceId = result->getString("resource_id");
    convId = result->getString("conv_id");
    uploaderUid = result->getInt("uploader_uid");
    md5 = result->getString("md5");
    fileSize = result->getInt64("file_size");
    fileName = result->getString("file_name");
    filePath = result->getString("file_path");
    if (!result->isNull("thumb_path"))
        thumbPath = result->getString("thumb_path");
    resourceType = static_cast<uint8_t>(result->getInt("resource_type"));
    status = static_cast<uint8_t>(result->getInt("status"));
    referenceCount = result->getInt("reference_count");
    if (!result->isNull("width")) width = result->getInt("width");
    if (!result->isNull("height")) height = result->getInt("height");
    if (!result->isNull("duration")) duration = result->getInt("duration");
    createTime = result->getString("create_time");
}

inline ResourceMeta ResourceMeta::fromResultListSet(const std::shared_ptr<sql::ResultSet> &result) {
    ResourceMeta meta;
    meta.fromResultSet(result);
    return meta;
}

#endif //IMSERVER_RESOURCEMETA_H
