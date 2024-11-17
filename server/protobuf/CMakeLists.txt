cmake_minimum_required(VERSION 3.28)
project(libprotocol)

include_directories(./protocol)
aux_source_directory(./protocol SRC_LIST)

find_package(protobuf REQUIRED PATHS ${THIRD_PARTY_PATH}/protobuf/lib)
add_library(protocol SHARED ${SRC_LIST})

message("Protobuf_LIBRARIES: ${Protobuf_LIBRARIES}")
message("Protobuf_FOUND: ${Protobuf_FOUND}")
target_link_libraries(protocol ${Protobuf_LIBRARIES})