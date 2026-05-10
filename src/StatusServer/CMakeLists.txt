set(PROTO_FILES message.proto)
set(PROTO_BIN_DIR ${CMAKE_CURRENT_SOURCE_DIR})

generate_grpc_code("${PROTO_FILES}" ${PROTO_BIN_DIR})

set(STATUS_SERVER_SOURCES
    main.cpp
    ${GENERATED_PROTO_SRCS}
    StatusServer.cpp
    StatusServer.h
    StatusServiceImpl.cpp
    StatusServiceImpl.h
)

add_executable(StatusServer ${STATUS_SERVER_SOURCES})

target_link_libraries(StatusServer
    common
    grpc_deps
)

target_include_directories(StatusServer
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${PROTO_BIN_DIR}
)

add_custom_command(TARGET StatusServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/config.ini
        $<TARGET_FILE_DIR:StatusServer>/config.ini
    COMMENT "Copying config.ini to runtime directory"
)

install(TARGETS StatusServer
    RUNTIME DESTINATION bin
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/config.ini DESTINATION bin)
