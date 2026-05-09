cmake_minimum_required(VERSION 3.15)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.30")
    cmake_policy(SET CMP0167 NEW)
endif()

project(IMServer
    VERSION 1.0.0
    DESCRIPTION "Instant Messaging Server Suite"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_GATE_SERVER "Build GateServer" ON)
option(BUILD_CHAT_SERVER "Build ChatServer" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)

find_package(Boost 1.90 REQUIRED COMPONENTS filesystem)

find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

add_library(jsoncpp_deps INTERFACE)
target_include_directories(jsoncpp_deps INTERFACE ${JSONCPP_INCLUDE_DIRS})
target_link_libraries(jsoncpp_deps INTERFACE ${JSONCPP_LINK_LIBRARIES})

find_package(mysql-concpp REQUIRED CONFIG COMPONENTS jdbc)
add_library(mysqlcppconn_deps INTERFACE)
target_link_libraries(mysqlcppconn_deps INTERFACE mysql::concpp-jdbc)

find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

add_library(grpc_deps INTERFACE)
target_link_libraries(grpc_deps INTERFACE
    gRPC::grpc++
    gRPC::grpc
    protobuf::libprotobuf
)

pkg_check_modules(HIREDIS REQUIRED hiredis)

add_library(hiredis_deps INTERFACE)
target_include_directories(hiredis_deps INTERFACE ${HIREDIS_INCLUDE_DIRS})
target_link_libraries(hiredis_deps INTERFACE ${HIREDIS_LINK_LIBRARIES})

if(BUILD_GATE_SERVER)
    add_subdirectory(src/GateServer)
endif()

if(BUILD_CHAT_SERVER)
    add_subdirectory(src/ChatServer)
endif()
