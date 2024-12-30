cmake_minimum_required(VERSION 3.28)
project(server_lib)

include_directories(. ./tcp_server ../base)
aux_source_directory(./ SRC_LIST)
aux_source_directory(./tcp_server SRC_LIST)

add_library(server_lib SHARED ${SRC_LIST}
        tcp_server/ServerConnectionLimiter.h)

find_package(Poco REQUIRED COMPONENTS Net Util XML JSON)
find_package(protobuf REQUIRED PATHS ${THIRD_PARTY_PATH}/protobuf/lib)
target_link_libraries(server_lib PRIVATE Poco::Net Poco::Util Poco::XML Poco::JSON protobuf base)
