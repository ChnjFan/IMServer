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

# Add include directories
include_directories(include ${Boost_INCLUDE_DIRS})

# Collect source files
file(GLOB_RECURSE CORE_SOURCES "src/core/*.cpp")
file(GLOB_RECURSE NETWORK_SOURCES "src/network/*.cpp")
file(GLOB_RECURSE PROTOCOL_SOURCES "src/protocol/*.cpp")
file(GLOB_RECURSE STORAGE_SOURCES "src/storage/*.cpp")
file(GLOB_RECURSE UTILS_SOURCES "src/utils/*.cpp")
file(GLOB_RECURSE MAIN_SOURCES "src/main/*.cpp")

# Combine all sources
set(SOURCES 
    ${CORE_SOURCES}
    ${NETWORK_SOURCES}
    ${PROTOCOL_SOURCES}
    ${STORAGE_SOURCES}
    ${UTILS_SOURCES}
    ${MAIN_SOURCES}
)

# Create executable
add_executable(imserver ${SOURCES})

# Link Boost libraries
target_link_libraries(imserver ${Boost_LIBRARIES})

# Add compiler warnings
if(MSVC)
    target_compile_options(imserver PRIVATE /W4 /WX)
else()
    target_compile_options(imserver PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Add debug symbols
set(CMAKE_BUILD_TYPE Debug)

# Install target
install(TARGETS imserver DESTINATION bin)
install(DIRECTORY config/ DESTINATION etc/imserver)

# Add tests
# enable_testing()
# add_subdirectory(test)