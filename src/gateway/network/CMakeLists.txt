cmake_minimum_required(VERSION 3.10)

# 设置C++标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Boost.Beast requires Boost version >= 1.70
set(BOOST_MIN_VERSION "1.70")

# 查找Boost库
find_package(Boost ${BOOST_MIN_VERSION} REQUIRED COMPONENTS system)

# 查找OpenSSL
find_package(OpenSSL REQUIRED)

# 设置头文件目录
set(HEADER_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tool/    # 包含工具层头文件
    ${CMAKE_CURRENT_SOURCE_DIR}/../protocol/    # 包含协议层头文件
)

# 包含目录
include_directories(${HEADER_DIR})
include_directories(${Boost_INCLUDE_DIRS})

# 源文件
set(SOURCES
    ConnectionManager.cpp
    EventLoop.cpp
    HttpServer.cpp
    TcpServer.cpp
    WebSocketServer.cpp
)

# 创建静态库
add_library(gateway_network STATIC ${SOURCES})

# 链接库
target_link_libraries(gateway_network
    ${Boost_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
)

# 设置编译选项
target_compile_options(gateway_network PRIVATE -Wall -Wextra)

# 如果Boost版本较新，使用C++11的async
if(Boost_VERSION GREATER_EQUAL "1.70")
    target_compile_definitions(gateway_network PRIVATE BOOST_ASIO_HAS_STD_CHRONO)
endif()

# 安装配置
install(TARGETS gateway_network
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES
    WebSocketServer.h
    HttpServer.h
    DESTINATION include/gateway/network
)