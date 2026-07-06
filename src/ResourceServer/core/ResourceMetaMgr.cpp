//
// Created by Fan on 2026/7/2.
//

#include "ResourceMetaMgr.h"

#include "ResourceMetaCache.h"

bool ResourceMetaMgr::preCheck(const std::string& md5, ResourceMeta& meta) {
    if (std::string resourceId; ResourceMetaCache::getInstance()->getByMd5(md5, resourceId)) {
        return getResource(resourceId, meta);
    }

    if (MysqlMgr::getInstance()->selectByMd5(md5, meta)) {
        ResourceMetaCache::getInstance()->cacheMd5(md5, meta.resourceId);
        ResourceMetaCache::getInstance()->set(meta);
        return true;
    }

    return false;
}

bool ResourceMetaMgr::registerResource(const ResourceMeta& meta) {
    if (!MysqlMgr::getInstance()->insertResource(meta)) {
        return false;
    }

    ResourceMetaCache::getInstance()->set(meta);
    if (!meta.md5.empty()) {
        ResourceMetaCache::getInstance()->cacheMd5(meta.md5, meta.resourceId);
    }
    return true;
}

bool ResourceMetaMgr::getResource(const std::string &resourceId, ResourceMeta &meta) {
    if (ResourceMetaCache::getInstance()->get(resourceId, meta)) {
        return true;
    }

    if (!MysqlMgr::getInstance()->selectByResourceId(resourceId, meta)) {
        return false;
    }
    ResourceMetaCache::getInstance()->set(meta);
    return true;
}

bool ResourceMetaMgr::acquire(const std::string& resourceId) {
    return MysqlMgr::getInstance()->updateRefCount(resourceId, 1);
}

bool ResourceMetaMgr::release(const std::string& resourceId) {
    return MysqlMgr::getInstance()->updateRefCount(resourceId, -1);
}

bool ResourceMetaMgr::updateThumbPath(const std::string& resourceId,
                                       const std::string& thumbPath) {
    if (!MysqlMgr::getInstance()->updateThumbPath(resourceId, thumbPath)) {
        return false;
    }

    if (ResourceMeta meta; MysqlMgr::getInstance()->selectByResourceId(resourceId, meta)) {
        ResourceMetaCache::getInstance()->set(meta);
        return true;
    }
    return false;
}