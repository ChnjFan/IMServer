set(PROTO_FILES message.proto)
set(PROTO_BIN_DIR ${CMAKE_CURRENT_SOURCE_DIR})

generate_grpc_code("${PROTO_FILES}" ${PROTO_BIN_DIR})

set(PROTO_CLIENT_SOURCES
    ${GENERATED_PROTO_SRCS}
    StatusGrpcClient.cpp
    StatusGrpcClient.h
    VerifyGrpcClient.cpp
    VerifyGrpcClient.h
)

add_library(proto_client STATIC ${PROTO_CLIENT_SOURCES})

target_link_libraries(proto_client
    grpc_deps
    base
)

target_include_directories(proto_client
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
