//
// Created by Fan on 2026/7/2.
//

#include "ResourceMetaCache.h"

#include "RedisMgr.h"


bool ResourceMetaCache::getByMd5(const std::string &md5, std::string &resourceId) const {
    return RedisMgr::getInstance()->get(md5Key(md5), resourceId);
}

void ResourceMetaCache::cacheMd5(const std::string& md5, const std::string& resourceId) const {
    RedisMgr::getInstance()->set(md5Key(md5), resourceId);
}

bool ResourceMetaCache::get(const std::string& resourceId, ResourceMeta& meta) const {
    std::string jsonStr;
    if (!RedisMgr::getInstance()->get(metaKey(resourceId), jsonStr) || jsonStr.empty()) {
        return false;
    }

    Json::Value root;
    if (Json::Reader reader; !reader.parse(jsonStr, root)) {
        return false;
    }

    meta.fromJson(root);
    return true;
}

void ResourceMetaCache::set(const ResourceMeta& meta) const {
    Json::Value root;
    meta.toJson(root);
    RedisMgr::getInstance()->set(metaKey(meta.resourceId), root.toStyledString());
}

void ResourceMetaCache::remove(const std::string& resourceId) const {
    if (!RedisMgr::getInstance()->del(metaKey(resourceId))) {
        std::cout << resourceId << " remove error" << std::endl;
    }
}
