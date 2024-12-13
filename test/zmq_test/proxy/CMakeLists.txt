cmake_minimum_required(VERSION 3.28)
project(proxy)

include_directories(. ../base)

aux_source_directory(./ SRC_LIST)

add_executable(proxy ${SRC_LIST})
set_target_properties(proxy PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/debug
)

find_package(Poco REQUIRED COMPONENTS Net Util XML JSON)
target_link_libraries(proxy PRIVATE Poco::Net Poco::Util Poco::XML Poco::JSON zmq)