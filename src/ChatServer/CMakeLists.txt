set(PROTO_FILES message.proto)
set(PROTO_BIN_DIR ${CMAKE_CURRENT_SOURCE_DIR})

generate_grpc_code("${PROTO_FILES}" ${PROTO_BIN_DIR})

set(CHAT_SERVER_SOURCES
    main.cpp
    ${GENERATED_PROTO_SRCS}
)

add_executable(ChatServer ${CHAT_SERVER_SOURCES})

target_link_libraries(ChatServer
    jsoncpp_deps
    grpc_deps
    hiredis_deps
    mysqlcppconn_deps
    Boost::filesystem
)

target_include_directories(ChatServer
    PRIVATE ${PROJECT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${PROTO_BIN_DIR}
)

install(TARGETS ChatServer
    RUNTIME DESTINATION bin
)
