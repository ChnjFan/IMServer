//
// Created by Fan on 2026/7/2.
//

#ifndef IMSERVER_RESOURCEMETAMGR_H
#define IMSERVER_RESOURCEMETAMGR_H

#include <optional>
#include <string>

#include "Singleton.h"
#include "common/model/ResourceMeta.h"
#include "db/mysql/MysqlMgr.h"

class ResourceMetaMgr : public Singleton<ResourceMetaMgr> {
    friend class Singleton<ResourceMetaMgr>;
public:
    ~ResourceMetaMgr() = default;

    // 秒传预检: MD5 → 已有资源直接返回成功
    bool preCheck(const std::string& md5, ResourceMeta& meta);

    // 注册资源（上传完成后调用）
    bool registerResource(const ResourceMeta& meta);

    // 查询元数据（Cache → DB）
    bool getResource(const std::string& resourceId, ResourceMeta& meta);

    // 引用计数管理
    bool acquire(const std::string& resourceId);   // +1
    bool release(const std::string& resourceId);   // -1

    // 更新缩略图路径（异步图片处理完成后调用）
    bool updateThumbPath(const std::string& resourceId, const std::string& thumbPath);

private:
    ResourceMetaMgr() = default;
};


#endif //IMSERVER_RESOURCEMETAMGR_H