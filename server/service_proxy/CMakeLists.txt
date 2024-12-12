cmake_minimum_required(VERSION 3.28)
project(service_proxy)

include_directories(. ../base)

aux_source_directory(./ SRC_LIST)

add_executable(service_proxy ${SRC_LIST})
set_target_properties(service_proxy PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/debug
)

find_package(Poco REQUIRED COMPONENTS Net Util XML JSON)
target_link_libraries(service_proxy PRIVATE Poco::Net Poco::Util Poco::XML Poco::JSON base)