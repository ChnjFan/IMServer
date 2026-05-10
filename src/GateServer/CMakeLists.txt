set(PROTO_FILES message.proto)
set(PROTO_BIN_DIR ${CMAKE_CURRENT_SOURCE_DIR})

generate_grpc_code("${PROTO_FILES}" ${PROTO_BIN_DIR})

set(GATE_SERVER_SOURCES
    main.cpp
    GateServer.cpp
    GateServer.h
    HttpConnection.cpp
    HttpConnection.h
    LogicSystem.cpp
    LogicSystem.h
    ${GENERATED_PROTO_SRCS}
    VerifyGrpcClient.cpp
    VerifyGrpcClient.h
    StatusGrpcClient.cpp
    StatusGrpcClient.h
)

add_executable(GateServer ${GATE_SERVER_SOURCES})

target_link_libraries(GateServer
    common
    grpc_deps
)

target_include_directories(GateServer
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${PROTO_BIN_DIR}
)

add_custom_command(TARGET GateServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/config.ini
        $<TARGET_FILE_DIR:GateServer>/config.ini
    COMMENT "Copying config.ini to runtime directory"
)

install(TARGETS GateServer
    RUNTIME DESTINATION bin
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/config.ini DESTINATION bin)
