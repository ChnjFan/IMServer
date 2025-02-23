cmake_minimum_required(VERSION 3.28)
project(libproto)

include_directories(.)
aux_source_directory(. SRC_LIST)

add_library(proto SHARED ${SRC_LIST})

find_package(Poco REQUIRED COMPONENTS Net Util XML JSON)
find_package(protobuf REQUIRED PATHS ${THIRD_PARTY_PATH}/grpc/lib)
find_package(gRPC REQUIRED PATHS ${THIRD_PARTY_PATH}/grpc/lib)

target_link_libraries(proto PRIVATE gRPC::grpc++ protobuf)
