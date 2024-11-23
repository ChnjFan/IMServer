cmake_minimum_required(VERSION 3.28)
project(login_server)

include_directories(. ../protobuf/protocol ../common ./lib)

aux_source_directory(./ SRC_LIST)
aux_source_directory(./lib LIB_SRC_LIST)

add_executable(login_server ${SRC_LIST})
set_target_properties(login_server PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/debug
)
add_library(login SHARED ${LIB_SRC_LIST})

find_package(Poco REQUIRED COMPONENTS Net Util XML JSON)
target_link_libraries(login_server PRIVATE Poco::Net Poco::Util Poco::XML Poco::JSON protocol common)