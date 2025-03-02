cmake_minimum_required(VERSION 3.28)
project(im_client)

include_directories(. ../src/proto)

aux_source_directory(./ SRC_LIST)

add_executable(im_client ${SRC_LIST})
set_target_properties(im_client PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/debug
)

find_package(Poco REQUIRED COMPONENTS Net Util XML JSON)
find_package(protobuf REQUIRED PATHS ${THIRD_PARTY_PATH}/grpc/lib)
find_package(gRPC REQUIRED PATHS ${THIRD_PARTY_PATH}/grpc/lib)

target_link_libraries(im_client PRIVATE Poco::Net Poco::Util Poco::XML Poco::JSON proto gRPC::grpc++ protobuf)