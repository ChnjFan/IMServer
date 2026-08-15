//
// Created by Fan on 2026/5/8.
//

#include "MysqlMgr.h"

bool MysqlMgr::selectByMd5(const std::string &md5, ResourceMeta &meta) const {
    return resourceMetaDao_.selectByMd5(md5, meta);
}

bool MysqlMgr::insertResource(const ResourceMeta &meta) const {
    return resourceMetaDao_.insert(meta);
}

bool MysqlMgr::selectByResourceId(const std::string &resourceId, ResourceMeta &meta) const {
    return resourceMetaDao_.selectByResourceId(resourceId, meta);
}

bool MysqlMgr::updateRefCount(const std::string &resourceId, int value) const {
    return resourceMetaDao_.updateRefCount(resourceId, value);
}

bool MysqlMgr::updateThumbPath(const std::string &resourceId, const std::string &thumbPath) const {
    return resourceMetaDao_.updateThumbPath(resourceId, thumbPath);
}





