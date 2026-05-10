set(COMMON_SOURCES
    AsioIOServicePool.cpp
    AsioIOServicePool.h
    ConfigMgr.cpp
    ConfigMgr.h
    MysqlDao.cpp
    MysqlDao.h
    MysqlMgr.cpp
    MysqlMgr.h
    RedisMgr.cpp
    RedisMgr.h
    Singleton.h
    const.h
)

add_library(common STATIC ${COMMON_SOURCES})

target_link_libraries(common
    jsoncpp_deps
    hiredis_deps
    mysqlcppconn_deps
    Boost::filesystem
)

target_include_directories(common
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)
