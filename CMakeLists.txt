# CMakeLists.txt for IMServer

# Minimum CMake version required
cmake_minimum_required(VERSION 3.10)

# Project name and version
project(IMServer VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find Boost library
find_package(Boost REQUIRED COMPONENTS system thread)

# Find OpenSSL library
find_package(OpenSSL REQUIRED)

# Add include directories
include_directories(include ${Boost_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR})

# Add compiler warnings
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Add debug symbols
set(CMAKE_BUILD_TYPE Debug)

# 各个模块作为单独进程构建
# 网络模块
add_subdirectory(src/network)
# 工具模块（需要时添加）
add_subdirectory(src/tool)
# 协议模块
add_subdirectory(src/protocol)

# 其他模块将在各自目录中配置为单独进程
# 核心模块（需要时添加）
# add_subdirectory(src/core)

# 存储模块（需要时添加）
# add_subdirectory(src/storage)

# 工具模块（需要时添加）
# add_subdirectory(src/utils)

# 主程序（需要时添加）
# add_subdirectory(src/main)

# Install configuration
install(DIRECTORY config/ DESTINATION etc/imserver)

# Add tests
# enable_testing()
# add_subdirectory(test)