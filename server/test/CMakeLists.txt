cmake_minimum_required(VERSION 3.28)
project(IMServerTest)

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/")
set(CMAKE_CXX_STANDARD 17)
set(THIRD_PARTY_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/")

add_definitions( -g -W -Wall -lpthread -lprotobuf)

include_directories(./ third_party/poco/include third_party/protobuf/include third_party/zeromq/include)
link_directories(./ third_party/poco/lib/ third_party/protobuf/lib/ third_party/zeromq/lib)

add_subdirectory(zmq_test)
