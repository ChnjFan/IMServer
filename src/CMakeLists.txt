cmake_minimum_required(VERSION 3.28)
project(im_server)

include_directories(.)

aux_source_directory(./ SRC_LIST)

add_executable(im_server ${SRC_LIST}
        util.h)
set_target_properties(im_server PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/debug
)

find_package(Poco REQUIRED COMPONENTS Net Util XML JSON)
target_link_libraries(im_server PRIVATE Poco::Net Poco::Util Poco::XML Poco::JSON protobuf)
