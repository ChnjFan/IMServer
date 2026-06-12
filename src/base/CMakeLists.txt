set(BASE_SOURCES
    AsioIOServicePool.cpp
    AsioIOServicePool.h
    ConfigMgr.cpp
    ConfigMgr.h
    MysqlPool.cpp
    MysqlPool.h
    RedisMgr.cpp
    RedisMgr.h
    Singleton.h
    const.h
    ThreadPool.cpp
    ThreadPool.h
    DistLock.cpp
    DistLock.h
)

add_library(base STATIC ${BASE_SOURCES})

target_link_libraries(base
    jsoncpp_deps
    hiredis_deps
    mysqlcppconn_deps
    Boost::filesystem
)

target_include_directories(base
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)
