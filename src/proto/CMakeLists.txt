set(PROTO_FILES message.proto)
set(PROTO_BIN_DIR ${CMAKE_CURRENT_SOURCE_DIR})

generate_grpc_code("${PROTO_FILES}" ${PROTO_BIN_DIR})

set(PROTO_SOURCES
    ${GENERATED_PROTO_SRCS}
    StatusGrpcClient.cpp
    StatusGrpcClient.h
    VerifyGrpcClient.cpp
    VerifyGrpcClient.h
)

add_library(proto STATIC ${PROTO_SOURCES})

target_link_libraries(proto
    grpc_deps
    base
)

target_include_directories(proto
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
