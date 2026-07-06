set(RESOURCE_SERVER_SOURCES
    main.cpp

    # net
        net/ResourceServer.cpp
        net/ResourceServer.h
    net/HttpConnection.cpp
    net/HttpConnection.h
        core/ResourceLogicSystem.cpp
        core/ResourceLogicSystem.h

    # service
    service/PreCheckHandler.cpp
    service/PreCheckHandler.h
    service/DownloadHandler.cpp
    service/DownloadHandler.h
    service/AuthMiddleware.cpp
    service/AuthMiddleware.h
    service/UploadSessionMgr.cpp
    service/UploadSessionMgr.h
    service/ChunkUploadHandler.cpp
    service/ChunkUploadHandler.h

    # common
    common/ResourceConfig.h
    common/model/ResourceMeta.h

    # core
    core/ResourceMetaMgr.cpp
    core/ResourceMetaMgr.h

    # db
    db/mysql/MysqlMgr.cpp
    db/mysql/MysqlMgr.h
    db/mysql/dao/ResourceMetaDao.cpp
    db/mysql/dao/ResourceMetaDao.h
    db/redis/ResourceMetaCache.cpp
    db/redis/ResourceMetaCache.h
)

add_executable(ResourceServer ${RESOURCE_SERVER_SOURCES})

add_dependencies(ResourceServer copy_config)

target_link_libraries(ResourceServer
    crypto_deps
    proto
    mysqlcppconn_deps
)

target_include_directories(ResourceServer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/net
        ${CMAKE_CURRENT_SOURCE_DIR}/service
        ${CMAKE_CURRENT_SOURCE_DIR}/core
        ${CMAKE_CURRENT_SOURCE_DIR}/common
        ${CMAKE_CURRENT_SOURCE_DIR}/db/mysql
        ${CMAKE_CURRENT_SOURCE_DIR}/db/redis
        ${CMAKE_CURRENT_SOURCE_DIR}/db/mysql/dao
)

install(TARGETS ResourceServer
    RUNTIME DESTINATION bin
)
