cmake_minimum_required(VERSION 3.28)
project(IMServer)

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/")
set(CMAKE_CXX_STANDARD 17)
set(THIRD_PARTY_PATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/")

add_definitions( -g -W -Wall -lpthread -lprotobuf)

include_directories(./ third_party/poco/include third_party/protobuf/include third_party/zeromq/include third_party/zookeeper/include)
link_directories(./ third_party/poco/lib/ third_party/protobuf/lib/ third_party/zeromq/lib third_party/zookeeper/lib)

# add_subdirectory(server/test)
# add_subdirectory(server/base)
# add_subdirectory(server/lib)
# add_subdirectory(server/protobuf)
# add_subdirectory(server/conn_server)
# add_subdirectory(server/account_server)
add_subdirectory(src)
add_subdirectory(src/proto)
add_subdirectory(example)
