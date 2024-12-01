cmake_minimum_required(VERSION 3.28)
project(conn_server)

include_directories(. ../protobuf/protocol ../common ../login_server/lib)

aux_source_directory(./ SRC_LIST)

add_executable(conn_server ${SRC_LIST})
set_target_properties(conn_server PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/debug
)

find_package(Poco REQUIRED COMPONENTS Net Util XML JSON)
target_link_libraries(conn_server PRIVATE Poco::Net Poco::Util Poco::XML Poco::JSON protocol common login)