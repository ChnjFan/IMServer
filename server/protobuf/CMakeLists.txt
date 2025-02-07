cmake_minimum_required(VERSION 3.28)
project(libprotocol)

include_directories(./protocol)
aux_source_directory(./protocol SRC_LIST)

add_library(protocol SHARED ${SRC_LIST})

find_package(Poco REQUIRED COMPONENTS Net Util XML JSON)
find_package(protobuf REQUIRED PATHS ${THIRD_PARTY_PATH}/grpc/lib)
find_package(gRPC REQUIRED PATHS ${THIRD_PARTY_PATH}/grpc/lib)

target_link_libraries(protocol PRIVATE gRPC::grpc++ protobuf)
