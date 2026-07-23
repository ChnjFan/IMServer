//
// Created by Fan on 2026/7/2.
//

#include "ResourceMetaDao.h"

#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/cppconn/resultset.h>

#include "ConfigMgr.h"
#include "MysqlPool.h"

ResourceMetaDao::ResourceMetaDao() {
    auto& conf = ConfigMgr::getInstance();
    const auto& host = conf["Mysql"]["Host"];
    const auto& port = conf["Mysql"]["Port"];
    const auto& user = conf["Mysql"]["User"];
    const auto& password = conf["Mysql"]["Password"];
    const auto& schema = conf["Mysql"]["Schema"];
    pool_ = std::make_unique<MysqlPool>(host + ":" + port, user, password, schema);
}

ResourceMetaDao::~ResourceMetaDao() {
    pool_->close();
}


bool ResourceMetaDao::insert(const ResourceMeta& meta) const {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });

    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
            "INSERT INTO resource_meta (resource_id, conv_id, uploader_uid, md5, "
            "file_size, file_name, file_path, thumb_path, resource_type, status, "
            "reference_count, width, height, duration) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));

        stmt->setString(1, meta.resourceId);
        stmt->setString(2, meta.convId);
        stmt->setInt(3, meta.uploaderUid);
        stmt->setString(4, meta.md5);
        stmt->setInt64(5, meta.fileSize);
        stmt->setString(6, meta.fileName);
        stmt->setString(7, meta.filePath);
        if (!meta.thumbPath.empty()) {
            stmt->setString(8, meta.thumbPath);
        }
        else {
            stmt->setNull(8, sql::DataType::VARCHAR);
        }
        stmt->setInt(9, static_cast<int>(meta.resourceType));
        stmt->setInt(10, static_cast<int>(meta.status));
        stmt->setInt(11, meta.referenceCount);
        if (meta.width > 0) {
            stmt->setInt(12, meta.width);
        }
        else {
            stmt->setNull(12, sql::DataType::INTEGER);
        }
        if (meta.height > 0) {
            stmt->setInt(13, meta.height);
        }
        else {
            stmt->setNull(13, sql::DataType::INTEGER);
        }
        if (meta.duration > 0) {
            stmt->setInt(14, meta.duration);
        }
        else {
            stmt->setNull(14, sql::DataType::INTEGER);
        }

        if (const int rowAffected = stmt->executeUpdate(); rowAffected < 0) {
            
            return false;
        }
        
        return true;
    } catch (sql::SQLException& e) {
        std::cerr << "SQL error in insert: " << e.what() << std::endl;
        return false;
    }
}

bool ResourceMetaDao::selectByResourceId(const std::string &resourceId, ResourceMeta &meta) const {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });

    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
            "SELECT * FROM resource_meta WHERE resource_id = ?"));
        stmt->setString(1, resourceId);

        if (const std::shared_ptr<sql::ResultSet> res(stmt->executeQuery()); res->next()) {
            
            meta.fromResultSet(res);
            return true;
        }
        
        return false;
    } catch (sql::SQLException& e) {
        std::cerr << "SQL error in selectByResourceId: " << e.what() << std::endl;
        return false;
    }
}

bool ResourceMetaDao::selectByMd5(const std::string &md5, ResourceMeta &meta) const {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });

    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
            "SELECT * FROM resource_meta WHERE md5 = ? AND status = 0 LIMIT 1"));
        stmt->setString(1, md5);

        if (const std::shared_ptr<sql::ResultSet> res(stmt->executeQuery()); res->next()) {
            
            meta.fromResultSet(res);
            return true;
        }
        
        return false;
    } catch (sql::SQLException& e) {
        std::cerr << "SQL error in selectByMd5: " << e.what() << std::endl;
        return false;
    }
}

std::vector<ResourceMeta> ResourceMetaDao::selectByConvId(const std::string& convId,
                                                           const int limit, const int offset) const {
    auto conn = pool_->getConnect();
    if (!conn) {
        return {};
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });

    try {
        std::vector<ResourceMeta> results;

        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
            "SELECT * FROM resource_meta WHERE conv_id = ? AND status = 0 "
            "ORDER BY create_time DESC LIMIT ? OFFSET ?"));
        stmt->setString(1, convId);
        stmt->setInt(2, limit);
        stmt->setInt(3, offset);

        const std::shared_ptr<sql::ResultSet> res(stmt->executeQuery());
        
        while (res->next()) {
            results.push_back(ResourceMeta::fromResultListSet(res));
        }
        return results;
    } catch (sql::SQLException& e) {
        std::cerr << "SQL error in selectByConvId: " << e.what() << std::endl;
        return {};
    }
}

bool ResourceMetaDao::updateRefCount(const std::string& resourceId, const int delta) const {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });

    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
            "UPDATE resource_meta SET reference_count = reference_count + ? "
            "WHERE resource_id = ?"));
        stmt->setInt(1, delta);
        stmt->setString(2, resourceId);
        const auto res = stmt->executeUpdate();
        
        return res > 0;
    } catch (sql::SQLException& e) {
        std::cerr << "SQL error in updateRefCount: " << e.what() << std::endl;
        return false;
    }
}

bool ResourceMetaDao::updateThumbPath(const std::string& resourceId,
                                       const std::string& thumbPath) const {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });

    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
            "UPDATE resource_meta SET thumb_path = ? WHERE resource_id = ?"));
        stmt->setString(1, thumbPath);
        stmt->setString(2, resourceId);
        const auto res = stmt->executeUpdate();
        
        return res > 0;
    } catch (sql::SQLException& e) {
        std::cerr << "SQL error in updateThumbPath: " << e.what() << std::endl;
        return false;
    }
}

bool ResourceMetaDao::updateStatus(const std::string& resourceId, ResourceStatus status) const {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });

    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
            "UPDATE resource_meta SET status = ? WHERE resource_id = ?"));
        stmt->setInt(1, static_cast<int>(status));
        stmt->setString(2, resourceId);
        const auto res = stmt->executeUpdate();
        
        return res > 0;
    } catch (sql::SQLException& e) {
        std::cerr << "SQL error in updateStatus: " << e.what() << std::endl;
        return false;
    }
}

bool ResourceMetaDao::remove(const std::string& resourceId) const {
    auto conn = pool_->getConnect();
    if (!conn) {
        return false;
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });

    try {
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
            "DELETE FROM resource_meta WHERE resource_id = ?"));
        stmt->setString(1, resourceId);
        const auto res = stmt->executeUpdate();
        
        return res > 0;
    } catch (sql::SQLException& e) {
        std::cerr << "SQL error in remove: " << e.what() << std::endl;
        return false;
    }
}

std::vector<ResourceMeta> ResourceMetaDao::selectZeroRefDeleted() const {
    auto conn = pool_->getConnect();
    if (!conn) {
        return {};
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });

    try {
        std::vector<ResourceMeta> results;
        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
            "SELECT * FROM resource_meta WHERE reference_count = 0 AND status = 2"));
        const std::shared_ptr<sql::ResultSet> res(stmt->executeQuery());
        
        while (res->next()) {
            results.push_back(ResourceMeta::fromResultListSet(res));
        }
        return results;
    } catch (sql::SQLException& e) {
        std::cerr << "SQL error in selectZeroRefDeleted: " << e.what() << std::endl;
        return {};
    }
}

std::vector<ResourceMeta> ResourceMetaDao::selectPotentiallyOrphan() const {
    auto conn = pool_->getConnect();
    if (!conn) {
        return {};
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });

    try {
        std::vector<ResourceMeta> results;

        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
            "SELECT * FROM resource_meta WHERE reference_count > 0 AND status = 0"));
        const std::shared_ptr<sql::ResultSet> res(stmt->executeQuery());
        
        while (res->next()) {
            results.push_back(ResourceMeta::fromResultListSet(res));
        }
        return results;
    } catch (sql::SQLException& e) {
        std::cerr << "SQL error in selectPotentiallyOrphan: " << e.what() << std::endl;
        return {};
    }
}

std::vector<ResourceMeta> ResourceMetaDao::selectStaleUploading(const int minutes) const {
    auto conn = pool_->getConnect();
    if (!conn) {
        return {};
    }
    Defer defer([this, &conn]() {
        pool_->returnConnect(std::move(conn));
    });

    try {
        std::vector<ResourceMeta> results;

        const std::unique_ptr<sql::PreparedStatement> stmt(conn->conn_->prepareStatement(
            "SELECT * FROM resource_meta WHERE status = 1 "
            "AND create_time < DATE_SUB(NOW(), INTERVAL ? MINUTE)"));
        stmt->setInt(1, minutes);
        const std::shared_ptr<sql::ResultSet> res(stmt->executeQuery());
        
        while (res->next()) {
            results.push_back(ResourceMeta::fromResultListSet(res));
        }
        return results;
    } catch (sql::SQLException& e) {
        std::cerr << "SQL error in selectStaleUploading: " << e.what() << std::endl;
        return {};
    }
}
