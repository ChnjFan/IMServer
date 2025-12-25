cmake_minimum_required(VERSION 3.10)
project(IMServer)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Boost.Beast requires Boost version >= 1.70
set(BOOST_MIN_VERSION "1.70")

# 查找Boost库
find_package(Boost ${BOOST_MIN_VERSION} REQUIRED COMPONENTS system)

# 查找OpenSSL
find_package(OpenSSL REQUIRED)

# 包含目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${Boost_INCLUDE_DIRS})

# 源文件
set(SOURCES
    WebSocketServer.cpp
    HttpServer.cpp
)

# 创建静态库
add_library(network STATIC ${SOURCES})

# 链接库
target_link_libraries(network
    ${Boost_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
)

# 设置编译选项
target_compile_options(network PRIVATE -Wall -Wextra)

# 如果Boost版本较新，使用C++11的async
if(Boost_VERSION GREATER_EQUAL "1.70")
    target_compile_definitions(network PRIVATE BOOST_ASIO_HAS_STD_CHRONO)
endif()

# 安装配置
install(TARGETS network
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES
    WebSocketServer.h
    HttpServer.h
    DESTINATION include/network
)