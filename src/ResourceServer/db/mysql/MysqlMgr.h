//
// Created by Fan on 2026/5/8.
//

#ifndef IMSERVER_MYSQLMGR_H
#define IMSERVER_MYSQLMGR_H

#include <vector>
#include <json/config.h>

#include "Singleton.h"
#include "const.h"

#include "common/model/ResourceMeta.h"
#include "db/mysql/dao/ResourceMetaDao.h"

class MysqlMgr : public Singleton<MysqlMgr> {
public:
    ~MysqlMgr() = default;

    bool selectByMd5(const std::string &md5, ResourceMeta &meta) const;

    bool insertResource(const ResourceMeta & meta) const;

    bool selectByResourceId(const std::string &resourceId, ResourceMeta & meta) const;

    bool updateRefCount(const std::string &resourceId, int value) const;

    bool updateThumbPath(const std::string &resourceId, const std::string & thumbPath) const;

private:
    friend class Singleton<MysqlMgr>;
    MysqlMgr() = default;

    ResourceMetaDao resourceMetaDao_;
};

#endif //IMSERVER_MYSQLMGR_H
