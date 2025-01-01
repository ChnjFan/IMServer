cmake_minimum_required(VERSION 3.28)
project(server_lib)

include_directories(. ../base)
aux_source_directory(./ SRC_LIST)

add_library(server_lib SHARED ${SRC_LIST})

find_package(Poco REQUIRED COMPONENTS Net Util XML JSON)
find_package(protobuf REQUIRED PATHS ${THIRD_PARTY_PATH}/protobuf/lib)
target_link_libraries(server_lib PRIVATE Poco::Net Poco::Util Poco::XML Poco::JSON protobuf base)
