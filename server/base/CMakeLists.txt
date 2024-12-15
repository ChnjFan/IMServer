cmake_minimum_required(VERSION 3.28)
project(libcommon)

include_directories(.)
aux_source_directory(./ SRC_LIST)

add_library(base SHARED ${SRC_LIST}
        Logger.h)

find_package(Poco REQUIRED COMPONENTS Net Util XML JSON)
find_package(protobuf REQUIRED PATHS ${THIRD_PARTY_PATH}/protobuf/lib)
target_link_libraries(base PRIVATE Poco::Net Poco::Util Poco::XML Poco::JSON protobuf zmq)
