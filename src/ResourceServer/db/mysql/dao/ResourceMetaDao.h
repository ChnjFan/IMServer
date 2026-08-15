//
// Created by Fan on 2026/7/2.
//

#ifndef IMSERVER_RESOURCEMETADAO_H
#define IMSERVER_RESOURCEMETADAO_H


#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "MysqlPool.h"
#include "common/model/ResourceMeta.h"

class ResourceMetaDao {
public:
    ResourceMetaDao();
    ~ResourceMetaDao();

    bool insert(const ResourceMeta& meta) const;
    bool selectByResourceId(const std::string &resourceId, ResourceMeta &meta) const;
    bool selectByMd5(const std::string &md5, ResourceMeta &meta) const;
    std::vector<ResourceMeta> selectByConvId(const std::string& convId, int limit, int offset) const;
    bool updateRefCount(const std::string& resourceId, int delta) const;
    bool updateThumbPath(const std::string& resourceId, const std::string& thumbPath) const;
    bool updateStatus(const std::string& resourceId, ResourceStatus status) const;
    bool remove(const std::string& resourceId) const;

    // Orphan scanner queries
    std::vector<ResourceMeta> selectZeroRefDeleted() const;
    std::vector<ResourceMeta> selectPotentiallyOrphan() const;
    std::vector<ResourceMeta> selectStaleUploading(int minutes) const;

private:
    std::unique_ptr<MysqlPool> pool_;
};


#endif //IMSERVER_RESOURCEMETADAO_H