cmake_minimum_required(VERSION 3.28)
project(IMServer)

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/")
set(CMAKE_CXX_STANDARD 17)

add_definitions( -g -W -Wall -lpthread)

include_directories(./ third_party/poco/include)
link_directories(./ third_party/poco/lib/)

add_subdirectory(server/route_server)
add_subdirectory(server/test)
