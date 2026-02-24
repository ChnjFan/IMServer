cmake_minimum_required(VERSION 3.10)

project(service_user CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖
find_package(Protobuf REQUIRED)
find_package(GRPC REQUIRED)

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/protocol/generated
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/protocol/lib
    ${Protobuf_INCLUDE_DIRS}
    ${GRPC_INCLUDE_DIRS}
)

# 源文件
set(SOURCE_FILES
    UserServiceImpl.cpp
    UserServiceImpl.h
    main.cpp
)

# 构建可执行文件
add_executable(service_user ${SOURCE_FILES})

# 链接库
target_link_libraries(service_user
    ${Protobuf_LIBRARIES}
    ${GRPC_LIBRARIES}
    ${GRPC_GRPCPP_LIBRARIES}
    pthread
    im_common_protocol
)

# 安装
install(TARGETS service_user DESTINATION bin)
