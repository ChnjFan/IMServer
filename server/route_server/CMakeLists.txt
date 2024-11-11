cmake_minimum_required(VERSION 3.28)
project(route_server)

set(EXECUTABLE_OUTPUT_PATH  ../bin)

aux_source_directory(./ SRC_LIST)

set(CMAKE_CXX_STANDARD 17)
add_definitions( -g -W -Wall -lpthread)

include_directories(./ ../../third_party/poco/include)
link_directories(./ ../../third_party/poco/lib)

add_executable(route_server ${SRC_LIST})
