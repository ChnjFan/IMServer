cmake_minimum_required(VERSION 3.28)
project(IMServer)

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/")
set(CMAKE_CXX_STANDARD 17)
set(THIRD_PARTY_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/")

add_definitions( -g -W -Wall -lpthread)

include_directories(./ third_party/poco/include third_party/protobuf/include)
link_directories(./ third_party/poco/lib/ third_party/protobuf/lib/)

add_subdirectory(server/protobuf)
add_subdirectory(server/route_server)
add_subdirectory(server/test)
