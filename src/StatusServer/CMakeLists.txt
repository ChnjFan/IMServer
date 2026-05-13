set(PROTO_FILES message.proto)
set(PROTO_BIN_DIR ${CMAKE_CURRENT_SOURCE_DIR})

generate_grpc_code("${PROTO_FILES}" ${PROTO_BIN_DIR})

set(STATUS_SERVER_SOURCES
    main.cpp
    ${PROTO_BIN_DIR}/message.pb.cc
    ${PROTO_BIN_DIR}/message.grpc.pb.cc
    StatusServer.cpp
    StatusServer.h
    StatusServiceImpl.cpp
    StatusServiceImpl.h
    MysqlDao.cpp
    MysqlDao.h
    MysqlMgr.cpp
    MysqlMgr.h
)

add_executable(StatusServer ${STATUS_SERVER_SOURCES})

add_dependencies(StatusServer copy_config)

target_link_libraries(StatusServer
    base
    grpc_deps
    mysqlcppconn_deps
)

target_include_directories(StatusServer
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${PROTO_BIN_DIR}
)

install(TARGETS StatusServer
    RUNTIME DESTINATION bin
)
