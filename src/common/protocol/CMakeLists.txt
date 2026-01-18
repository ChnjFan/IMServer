cmake_minimum_required(VERSION 3.10)

project(im_common_protocol)

# 查找Protobuf包
find_package(Protobuf REQUIRED)

# 查找gRPC包
find_package(gRPC REQUIRED)

# 设置protobuf生成目录
set(PROTOBUF_GENERATE_CPP_APPEND_PATH FALSE)
set(PROTO_GEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/generated)

# 确保生成目录存在
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

# 编译protobuf文件
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS 
    OUTPUT_DIR ${PROTO_GEN_DIR}
    gateway_routing.proto
)

# 编译gRPC文件
grpc_generate_cpp(GRPC_SRCS GRPC_HDRS 
    OUTPUT_DIR ${PROTO_GEN_DIR}
    gateway_routing.proto
)

# 创建公共协议库
add_library(im_common_protocol
    ${PROTO_SRCS}
    ${PROTO_HDRS}
    ${GRPC_SRCS}
    ${GRPC_HDRS}
)

# 设置包含目录
target_include_directories(im_common_protocol
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
        ${Protobuf_INCLUDE_DIRS}
        ${gRPC_INCLUDE_DIRS}
)

# 链接依赖
target_link_libraries(im_common_protocol
    ${Protobuf_LIBRARIES}
    gRPC::grpc++
)

# 设置编译选项
target_compile_features(im_common_protocol PRIVATE cxx_std_17)

# 安装配置
install(TARGETS im_common_protocol
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${PROTO_HDRS} ${GRPC_HDRS}
    DESTINATION include/im/common/protocol
)
