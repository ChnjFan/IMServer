cmake_minimum_required(VERSION 3.10)

project(im_routing_service VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 包含头文件目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/common/protocol/generated
    ${CMAKE_SOURCE_DIR}/src/common/protocol
    ${Protobuf_INCLUDE_DIRS}
    ${gRPC_INCLUDE_DIRS}
)

# 查找依赖
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(OpenSSL REQUIRED)

# 源文件
set(SOURCE_FILES
    RoutingService.cpp
    RoutingService.h
    MessageRouter.cpp
    MessageRouter.h
    ServiceDiscovery.cpp
    ServiceDiscovery.h
    LoadBalancer.cpp
    LoadBalancer.h
    MessageQueue.cpp
    MessageQueue.h
    Metrics.cpp
    Metrics.h
    main.cpp
)

# 链接库
set(LIBS
    im_common_protocol
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF}
    ${Boost_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    -lpthread
)

# 创建可执行文件
add_executable(im_routing_service ${SOURCE_FILES})

# 链接依赖
target_link_libraries(im_routing_service ${LIBS})

# 设置编译选项
if(MSVC)
    target_compile_options(im_routing_service PRIVATE /W4 /WX)
else()
    target_compile_options(im_routing_service PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# 调试配置
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(im_routing_service PRIVATE -ggdb -O0)
endif()

# 安装配置
install(TARGETS im_routing_service
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# 安装配置文件
install(DIRECTORY config/ DESTINATION etc/imserver/routing)
