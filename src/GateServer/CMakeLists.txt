set(PROTO_FILES message.proto)

set(PROTO_SRC_DIR ${PROJECT_SOURCE_DIR}/proto)
set(PROTO_BIN_DIR ${CMAKE_CURRENT_SOURCE_DIR})

foreach(proto_file ${PROTO_FILES})
    get_filename_component(proto_name ${proto_file} NAME_WE)
    
    set(generated_srcs
        ${PROTO_BIN_DIR}/${proto_name}.pb.cc
        ${PROTO_BIN_DIR}/${proto_name}.grpc.pb.cc
    )
    set(generated_hdrs
        ${PROTO_BIN_DIR}/${proto_name}.pb.h
        ${PROTO_BIN_DIR}/${proto_name}.grpc.pb.h
    )
    
    add_custom_command(
        OUTPUT ${generated_srcs} ${generated_hdrs}
        COMMAND protobuf::protoc
        ARGS 
            --grpc_out=${PROTO_BIN_DIR}
            --cpp_out=${PROTO_BIN_DIR}
            -I${PROTO_SRC_DIR}
            --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
            ${PROTO_SRC_DIR}/${proto_file}
        MAIN_DEPENDENCY ${PROTO_SRC_DIR}/${proto_file}
        COMMENT "Generating gRPC code from ${proto_file}"
        VERBATIM
    )
    
    list(APPEND GENERATED_PROTO_SRCS ${generated_srcs})
    list(APPEND GENERATED_PROTO_HDRS ${generated_hdrs})
endforeach()

set(GATE_SERVER_SOURCES
    main.cpp
    GateServer.cpp
    GateServer.h
    const.h
    HttpConnection.cpp
    HttpConnection.h
    Singleton.h
    LogicSystem.cpp
    LogicSystem.h
    ${GENERATED_PROTO_SRCS}
        VerifyGrpcClient.cpp
        VerifyGrpcClient.h
        ConfigMgr.cpp
        ConfigMgr.h
        AsioIOServicePool.cpp
        AsioIOServicePool.h
        RedisMgr.cpp
        RedisMgr.h
)

add_executable(GateServer ${GATE_SERVER_SOURCES})

target_link_libraries(GateServer
    jsoncpp_deps
    grpc_deps
    hiredis_deps
    Boost::filesystem
)

target_include_directories(GateServer
    PRIVATE ${PROJECT_SOURCE_DIR}/include
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
