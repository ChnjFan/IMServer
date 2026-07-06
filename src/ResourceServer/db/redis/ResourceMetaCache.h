//
// Created by Fan on 2026/7/2.
//

#ifndef IMSERVER_RESOURCEMETACACHE_H
#define IMSERVER_RESOURCEMETACACHE_H


#include <optional>
#include <string>

#include "Singleton.h"
#include "common/model/ResourceMeta.h"

#define RESOURCE_MD5_PREFIX      "resource_md5_"
#define RESOURCE_META_PREFIX     "resource_meta_"
#define CONV_RESOURCES_PREFIX    "conv_resources_"

class ResourceMetaCache : public Singleton<ResourceMetaCache> {
    friend class Singleton<ResourceMetaCache>;
public:
    ~ResourceMetaCache() = default;

    // MD5 → resource_id (秒传索引)
    bool getByMd5(const std::string& md5, std::string& resourceId) const;
    void cacheMd5(const std::string& md5, const std::string& resourceId) const;

    // resource_id → ResourceMeta
    bool get(const std::string& resourceId, ResourceMeta& meta) const;
    void set(const ResourceMeta& meta) const;
    void remove(const std::string& resourceId) const;

private:
    ResourceMetaCache() = default;

    static std::string metaKey(const std::string& resourceId) {
        return std::string(RESOURCE_META_PREFIX) + resourceId;
    }

    static std::string md5Key(const std::string& md5) {
        return std::string(RESOURCE_MD5_PREFIX) + md5;
    }
};


#endif //IMSERVER_RESOURCEMETACACHE_H