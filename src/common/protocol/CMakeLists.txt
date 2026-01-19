cmake_minimum_required(VERSION 3.10)

project(im_common_protocol)

# 查找Protobuf包
find_package(Protobuf REQUIRED)

# 查找gRPC包
find_package(gRPC REQUIRED)

# 确保gRPC插件存在
if(NOT DEFINED gRPC_CPP_PLUGIN_EXECUTABLE)
    # 尝试常见的插件路径
    find_program(gRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin
        PATHS 
            /usr/local/bin
            /usr/bin
            $ENV{PATH}
    )
    
    if(NOT gRPC_CPP_PLUGIN_EXECUTABLE)
        message(FATAL_ERROR "grpc_cpp_plugin not found. Please install gRPC or set gRPC_CPP_PLUGIN_EXECUTABLE explicitly.")
    endif()
endif()

message(STATUS "Found gRPC plugin: ${gRPC_CPP_PLUGIN_EXECUTABLE}")

# 设置protobuf生成目录
set(PROTOBUF_GENERATE_CPP_APPEND_PATH FALSE)
set(PROTO_GEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/generated)

# 确保生成目录存在
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

# 定义protobuf文件
set(PROTO_FILES gateway_routing.proto)

# 设置protobuf输出目录
set(protobuf_generate_PROTOC_OUT_DIR ${PROTO_GEN_DIR})

# 编译protobuf文件
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS 
    ${PROTO_FILES}
)

# 手动设置gRPC生成
set(GRPC_SRCS "")
set(GRPC_HDRS "")
foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
    list(APPEND GRPC_SRCS "${PROTO_GEN_DIR}/${PROTO_NAME}.grpc.pb.cc")
    list(APPEND GRPC_HDRS "${PROTO_GEN_DIR}/${PROTO_NAME}.grpc.pb.h")
    
    add_custom_command(
        OUTPUT "${PROTO_GEN_DIR}/${PROTO_NAME}.grpc.pb.cc" "${PROTO_GEN_DIR}/${PROTO_NAME}.grpc.pb.h"
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        ARGS -I=${CMAKE_CURRENT_SOURCE_DIR} --grpc_out=${PROTO_GEN_DIR} --plugin=protoc-gen-grpc=${gRPC_CPP_PLUGIN_EXECUTABLE} ${PROTO_FILE}
        DEPENDS ${PROTO_FILE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endforeach()

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
    -lutf8_range
    -labsl_log_internal_check_op
    -labsl_log_internal_message
    -labsl_log_internal_nullguard
)

# 设置编译选项
target_compile_features(im_common_protocol PRIVATE cxx_std_17)

# 添加编译选项来减少警告
target_compile_options(im_common_protocol PRIVATE
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-unused-result>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-relocation>
)

# 安装配置
install(TARGETS im_common_protocol
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${PROTO_HDRS} ${GRPC_HDRS}
    DESTINATION include/im/common/protocol
)
